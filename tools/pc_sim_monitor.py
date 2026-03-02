import argparse
import re
import socket
import sys
import time
from pathlib import Path
from typing import Dict, List, Optional

from PyQt5.QtCore import QTimer
from PyQt5.QtWidgets import (
    QApplication,
    QCheckBox,
    QFormLayout,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QPushButton,
    QSpinBox,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

from pc_sim_client import PcSimClient


def _resolve_fmkio_public_header() -> Path:
    repo_root = Path(__file__).resolve().parents[2]
    candidates = [
        repo_root / "src/1_FMK/FMK_CFG/FMKCFG_ConfigFiles/FMKIO_ConfigPublic.h",
        repo_root / "src/1_FMK/FMK_CFG/FMKCFG_ConfigFiles/FMKIO_COnfigPublic.h",
    ]
    for path in candidates:
        if path.exists():
            return path
    return candidates[-1]


def _extract_enum_blocks(content: str) -> List[str]:
    return re.findall(r"typedef\s+enum\s*\{(.*?)\}\s*\w+\s*;", content, flags=re.S)


def _count_from_enum_nb(content: str, nb_token: str) -> int:
    for block in _extract_enum_blocks(content):
        names = re.findall(r"\b([A-Za-z_]\w*)\b(?=\s*(?:=|,))", block)
        if nb_token in names:
            return names.index(nb_token)
    return 0


def _count_prefixed_items(content: str, prefix: str) -> int:
    matches = re.findall(rf"\b{re.escape(prefix)}(\d+)\b", content)
    if not matches:
        return 0
    return len({int(m) for m in matches})


def _load_fmkio_counts() -> Dict[str, int]:
    counts = {
        "ana": 1,
        "in_dig": 1,
        "out_dig": 1,
        "in_freq": 1,
        "enc": 1,
        "pwm": 1,
    }
    header = _resolve_fmkio_public_header()
    try:
        content = header.read_text(encoding="utf-8", errors="ignore")
    except Exception:
        return counts

    counts["ana"] = _count_from_enum_nb(content, "FMKIO_INPUT_SIGANA_NB")
    counts["in_dig"] = _count_from_enum_nb(content, "FMKIO_INPUT_SIGDIG_NB")
    counts["out_dig"] = _count_from_enum_nb(content, "FMKIO_OUTPUT_SIGDIG_NB")
    counts["in_freq"] = _count_from_enum_nb(content, "FMKIO_INPUT_SIGFREQ_NB")
    counts["enc"] = _count_from_enum_nb(content, "FMKIO_INPUT_ENCODER_NB")
    counts["pwm"] = _count_from_enum_nb(content, "FMKIO_OUTPUT_SIGPWM_NB")

    if counts["ana"] == 0:
        counts["ana"] = _count_prefixed_items(content, "FMKIO_INPUT_SIGANA_")
    if counts["in_dig"] == 0:
        counts["in_dig"] = _count_prefixed_items(content, "FMKIO_INPUT_SIGDIG_")
    if counts["out_dig"] == 0:
        counts["out_dig"] = _count_prefixed_items(content, "FMKIO_OUTPUT_SIGDIG_")
    if counts["in_freq"] == 0:
        counts["in_freq"] = _count_prefixed_items(content, "FMKIO_INPUT_SIGFREQ_")
    if counts["enc"] == 0:
        counts["enc"] = _count_prefixed_items(content, "FMKIO_INPUT_ENCODER_")
    if counts["pwm"] == 0:
        counts["pwm"] = _count_prefixed_items(content, "FMKIO_OUTPUT_SIGPWM_")

    # Keep at least 1 so monitor remains operable with PCSIM virtual slots.
    for k in counts:
        counts[k] = max(1, counts[k])
    return counts


def _format_can_frame(frame: Dict[str, object]) -> str:
    can_id = int(frame["can_id"])
    dlc = int(frame["dlc"])
    data: List[int] = list(frame["data"])
    payload = " ".join(f"{b:02X}" for b in data[:dlc])
    return (
        f"TS={int(frame['timestamp_ms']):>8}ms "
        f"NODE={int(frame['node'])} "
        f"ID=0x{can_id:08X} "
        f"{'EXT' if bool(frame['is_extended']) else 'STD'} "
        f"DLC={dlc} DATA=[{payload}]"
    )


class PcSimMonitorWindow(QMainWindow):
    def __init__(self, host: str, port: int) -> None:
        super().__init__()
        self.setWindowTitle("PC_SIM Monitor (PyQt5)")
        self.resize(1100, 760)

        self.client = PcSimClient(host=host, port=port, timeout=0.5)
        self.connected = False
        self.cycle = 0
        self.dig_state = 0
        self.last_warn_ts = 0.0
        self.counts = _load_fmkio_counts()

        self.timer = QTimer(self)
        self.timer.timeout.connect(self._tick)

        self.host_edit = QLineEdit(host)
        self.port_edit = QLineEdit(str(port))
        self.period_spin = QSpinBox()
        self.period_spin.setRange(10, 5000)
        self.period_spin.setValue(200)
        self.max_can_pop_spin = QSpinBox()
        self.max_can_pop_spin.setRange(1, 1000)
        self.max_can_pop_spin.setValue(100)

        self.clear_on_start_cb = QCheckBox("Clear CAN TX on start")
        self.clear_on_start_cb.setChecked(True)
        self.stimulate_cb = QCheckBox("Stimulate")
        self.stimulate_cb.setChecked(False)

        self.in_dig_idx_spin = QSpinBox()
        self.out_dig_idx_spin = QSpinBox()
        self.in_freq_idx_spin = QSpinBox()
        self.ana_idx_spin = QSpinBox()
        self.enc_idx_spin = QSpinBox()
        self.pwm_idx_spin = QSpinBox()
        self.ana_idx_spin.setRange(0, self.counts["ana"] - 1)
        self.in_dig_idx_spin.setRange(0, self.counts["in_dig"] - 1)
        self.out_dig_idx_spin.setRange(0, self.counts["out_dig"] - 1)
        self.in_freq_idx_spin.setRange(0, self.counts["in_freq"] - 1)
        self.enc_idx_spin.setRange(0, self.counts["enc"] - 1)
        self.pwm_idx_spin.setRange(0, self.counts["pwm"] - 1)
        self.ana_idx_spin.setValue(0)
        self.in_dig_idx_spin.setValue(0)
        self.out_dig_idx_spin.setValue(0)
        self.in_freq_idx_spin.setValue(0)
        self.enc_idx_spin.setValue(0)
        self.pwm_idx_spin.setValue(0)

        self.status_label = QLabel("Disconnected")
        self.cycle_label = QLabel("0")
        self.ana_label = QLabel("-")
        self.in_dig_label = QLabel("-")
        self.out_dig_label = QLabel("-")
        self.in_freq_label = QLabel("-")
        self.enc_label = QLabel("-")
        self.pwm_label = QLabel("-")
        self.can_pending_label = QLabel("0")

        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)

        self._build_ui()

    def _build_ui(self) -> None:
        root = QWidget()
        self.setCentralWidget(root)
        layout = QVBoxLayout(root)

        top = QHBoxLayout()
        top.addWidget(QLabel("Host"))
        top.addWidget(self.host_edit)
        top.addWidget(QLabel("Port"))
        top.addWidget(self.port_edit)
        top.addWidget(QLabel("Period (ms)"))
        top.addWidget(self.period_spin)
        top.addWidget(QLabel("Max CAN pop"))
        top.addWidget(self.max_can_pop_spin)
        start_btn = QPushButton("Start")
        stop_btn = QPushButton("Stop")
        start_btn.clicked.connect(self._start)
        stop_btn.clicked.connect(self._stop)
        top.addWidget(start_btn)
        top.addWidget(stop_btn)
        top.addWidget(self.clear_on_start_cb)
        top.addWidget(self.stimulate_cb)
        top.addStretch(1)
        layout.addLayout(top)

        idx_group = QGroupBox("Indexes")
        idx_form = QFormLayout(idx_group)
        idx_form.addRow("Analog idx", self.ana_idx_spin)
        idx_form.addRow("Input Dig idx", self.in_dig_idx_spin)
        idx_form.addRow("Output Dig idx", self.out_dig_idx_spin)
        idx_form.addRow("Input Freq idx", self.in_freq_idx_spin)
        idx_form.addRow("Encoder idx", self.enc_idx_spin)
        idx_form.addRow("PWM idx", self.pwm_idx_spin)

        status_group = QGroupBox("Status")
        status_grid = QGridLayout(status_group)
        status_grid.addWidget(QLabel("Connection"), 0, 0)
        status_grid.addWidget(self.status_label, 0, 1)
        status_grid.addWidget(QLabel("Cycle"), 1, 0)
        status_grid.addWidget(self.cycle_label, 1, 1)
        status_grid.addWidget(QLabel("CAN pending"), 2, 0)
        status_grid.addWidget(self.can_pending_label, 2, 1)
        status_grid.addWidget(QLabel("ANA"), 3, 0)
        status_grid.addWidget(self.ana_label, 3, 1)
        status_grid.addWidget(QLabel("IN_DIG"), 4, 0)
        status_grid.addWidget(self.in_dig_label, 4, 1)
        status_grid.addWidget(QLabel("OUT_DIG"), 5, 0)
        status_grid.addWidget(self.out_dig_label, 5, 1)
        status_grid.addWidget(QLabel("IN_FREQ"), 6, 0)
        status_grid.addWidget(self.in_freq_label, 6, 1)
        status_grid.addWidget(QLabel("ENC"), 7, 0)
        status_grid.addWidget(self.enc_label, 7, 1)
        status_grid.addWidget(QLabel("PWM"), 8, 0)
        status_grid.addWidget(self.pwm_label, 8, 1)

        mid = QHBoxLayout()
        mid.addWidget(idx_group)
        mid.addWidget(status_group, 1)
        layout.addLayout(mid)

        log_group = QGroupBox("Live Log")
        log_layout = QVBoxLayout(log_group)
        log_layout.addWidget(self.log_text)
        layout.addWidget(log_group, 1)

    def _log(self, msg: str) -> None:
        self.log_text.append(msg)

    def _apply_connection(self) -> None:
        host = self.host_edit.text().strip() or "127.0.0.1"
        port = int(self.port_edit.text().strip(), 0)
        timeout_s = max(0.2, self.period_spin.value() / 1000.0)
        self.client = PcSimClient(host=host, port=port, timeout=timeout_s)

    def _try_connect(self) -> bool:
        try:
            self._apply_connection()
            self.client.ping()
            if self.clear_on_start_cb.isChecked():
                self.client.clear_can_tx()
            self.connected = True
            self.status_label.setText("Connected")
            self._log("[INFO] connected")
            return True
        except Exception as exc:
            self.connected = False
            self.status_label.setText("Disconnected")
            now = time.time()
            if (now - self.last_warn_ts) > 1.0:
                self._log(f"[WARN] connect failed: {exc}")
                self.last_warn_ts = now
            return False

    def _start(self) -> None:
        self.cycle = 0
        self.dig_state = 0
        if not self._try_connect():
            self._log("[INFO] monitor started in reconnect mode")
        self.timer.start(self.period_spin.value())

    def _stop(self) -> None:
        self.timer.stop()
        self.connected = False
        self.status_label.setText("Stopped")
        self._log("[INFO] monitor stopped")

    def _tick(self) -> None:
        if not self.connected and not self._try_connect():
            return

        self.cycle += 1
        self.cycle_label.setText(str(self.cycle))

        ana_idx = self.ana_idx_spin.value()
        in_dig_idx = self.in_dig_idx_spin.value()
        out_dig_idx = self.out_dig_idx_spin.value()
        in_freq_idx = self.in_freq_idx_spin.value()
        enc_idx = self.enc_idx_spin.value()
        pwm_idx = self.pwm_idx_spin.value()

        try:
            if self.stimulate_cb.isChecked():
                self.dig_state = 1 - self.dig_state
                in_freq = 50.0 + float(self.cycle % 20) * 5.0
                enc_abs = float(self.cycle) * 0.1
                enc_rel = float(self.cycle) * 0.05
                enc_speed = 10.0 + float(self.cycle % 10)
                pwm_freq = 1000.0 + float((self.cycle % 8) * 250)
                pwm_pulses = 10 + (self.cycle % 40)
                ana_value = 1000.0 + float((self.cycle % 10) * 100)

                self.client.set_ana(ana_idx, ana_value)
                self.client.set_in_dig(in_dig_idx, self.dig_state)
                self.client.set_out_dig(out_dig_idx, self.dig_state)
                self.client.set_in_freq(in_freq_idx, in_freq)
                self.client.set_enc_pos(enc_idx, enc_abs, enc_rel)
                self.client.set_enc_speed(enc_idx, enc_speed)
                self.client.set_pwm_freq(pwm_idx, pwm_freq)
                self.client.set_pwm_pulses(pwm_idx, pwm_pulses)

            ana = self.client.get_ana(ana_idx)
            in_dig = self.client.get_in_dig(in_dig_idx)
            out_dig = self.client.get_out_dig(out_dig_idx)
            in_freq = self.client.get_in_freq(in_freq_idx)
            enc_abs, enc_rel = self.client.get_enc_pos(enc_idx)
            enc_speed = self.client.get_enc_speed(enc_idx)
            pwm = self.client.get_pwm(pwm_idx)
            pwm_freq = self.client.get_pwm_freq(pwm_idx)
            pwm_pulses = self.client.get_pwm_pulses(pwm_idx)

            self.ana_label.setText(f"{ana:.3f}")
            self.in_dig_label.setText(str(in_dig))
            self.out_dig_label.setText(str(out_dig))
            self.in_freq_label.setText(f"{in_freq:.2f} Hz")
            self.enc_label.setText(f"abs={enc_abs:.3f} rel={enc_rel:.3f} spd={enc_speed:.3f}")
            self.pwm_label.setText(f"duty={pwm} freq={pwm_freq:.2f}Hz pulses={pwm_pulses}")

            tx_count = self.client.get_can_tx_count()
            self.can_pending_label.setText(str(tx_count))

            max_pop = self.max_can_pop_spin.value()
            popped = 0
            while popped < max_pop:
                frame = self.client.pop_can_tx()
                if frame is None:
                    break
                popped += 1
                self._log(_format_can_frame(frame))

            if popped >= max_pop:
                self._log(f"... capped to {max_pop} CAN frames this cycle")

        except (TimeoutError, socket.timeout):
            self.connected = False
            self.status_label.setText("Reconnecting...")
        except Exception as exc:
            self.connected = False
            self.status_label.setText("Reconnecting...")
            self._log(f"[ERR] {exc}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=19090)
    args = parser.parse_args()

    app = QApplication(sys.argv)
    win = PcSimMonitorWindow(args.host, args.port)
    win.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
