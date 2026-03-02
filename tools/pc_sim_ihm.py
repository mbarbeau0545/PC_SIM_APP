import argparse
import re
import socket
import sys
from pathlib import Path
from typing import Dict, List

from PyQt5.QtCore import QTimer
from PyQt5.QtWidgets import (
    QApplication,
    QCheckBox,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QScrollArea,
    QSplitter,
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


def _count_prefixed_items(content: str, prefix: str) -> int:
    matches = re.findall(rf"\b{re.escape(prefix)}(\d+)\b", content)
    if not matches:
        return 0
    return len({int(m) for m in matches})


def _extract_enum_blocks(content: str) -> List[str]:
    return re.findall(r"typedef\s+enum\s*\{(.*?)\}\s*\w+\s*;", content, flags=re.S)


def _count_from_enum_nb(content: str, nb_token: str) -> int:
    enum_blocks = _extract_enum_blocks(content)
    for block in enum_blocks:
        # Collect identifiers in declaration order.
        names = re.findall(r"\b([A-Za-z_]\w*)\b(?=\s*(?:=|,))", block)
        if nb_token in names:
            idx = names.index(nb_token)
            if idx >= 0:
                return idx
    return 0


def _load_fmkio_counts() -> Dict[str, int]:
    counts = {
        "ana": 0,
        "pwm": 0,
        "in_dig": 0,
        "out_dig": 0,
        "in_freq": 0,
        "enc": 0,
    }
    header_path = _resolve_fmkio_public_header()
    try:
        content = header_path.read_text(encoding="utf-8", errors="ignore")
    except Exception:
        return counts

    # Priority: infer from enum *_NB token position.
    counts["ana"] = _count_from_enum_nb(content, "FMKIO_INPUT_SIGANA_NB")
    counts["pwm"] = _count_from_enum_nb(content, "FMKIO_OUTPUT_SIGPWM_NB")
    counts["in_dig"] = _count_from_enum_nb(content, "FMKIO_INPUT_SIGDIG_NB")
    counts["out_dig"] = _count_from_enum_nb(content, "FMKIO_OUTPUT_SIGDIG_NB")
    counts["in_freq"] = _count_from_enum_nb(content, "FMKIO_INPUT_SIGFREQ_NB")
    counts["enc"] = _count_from_enum_nb(content, "FMKIO_INPUT_ENCODER_NB")

    # Fallback: count numbered entries if NB token cannot be inferred.
    if counts["ana"] == 0:
        counts["ana"] = _count_prefixed_items(content, "FMKIO_INPUT_SIGANA_")
    if counts["pwm"] == 0:
        counts["pwm"] = _count_prefixed_items(content, "FMKIO_OUTPUT_SIGPWM_")
    if counts["in_dig"] == 0:
        counts["in_dig"] = _count_prefixed_items(content, "FMKIO_INPUT_SIGDIG_")
    if counts["out_dig"] == 0:
        counts["out_dig"] = _count_prefixed_items(content, "FMKIO_OUTPUT_SIGDIG_")
    if counts["in_freq"] == 0:
        counts["in_freq"] = _count_prefixed_items(content, "FMKIO_INPUT_SIGFREQ_")
    if counts["enc"] == 0:
        counts["enc"] = _count_prefixed_items(content, "FMKIO_INPUT_ENCODER_")
    return counts


def _infer_ana_pwm_counts_from_get_all(client: PcSimClient) -> Dict[str, int]:
    counts = {"ana": 0, "pwm": 0}
    try:
        rsp = client.get_all()
        parts = rsp.split()
        if "ANA" in parts and "PWM" in parts:
            ana_idx = parts.index("ANA")
            pwm_idx = parts.index("PWM")
            if pwm_idx > ana_idx:
                counts["ana"] = max(0, pwm_idx - ana_idx - 1)
                counts["pwm"] = max(0, len(parts) - pwm_idx - 1)
    except Exception:
        pass
    return counts


class PcSimIhmWindow(QMainWindow):
    def __init__(self, host: str, port: int) -> None:
        super().__init__()
        self.setWindowTitle("PC_SIM IHM (PyQt5)")
        self.resize(1200, 760)

        self.client = PcSimClient(host=host, port=port)
        self.refresh_timer = QTimer(self)
        self.refresh_timer.timeout.connect(self._refresh_once)
        self.refresh_timer.start(300)

        self.counts = _load_fmkio_counts()
        inferred = _infer_ana_pwm_counts_from_get_all(self.client)
        if self.counts["ana"] == 0:
            self.counts["ana"] = inferred["ana"]
        if self.counts["pwm"] == 0:
            self.counts["pwm"] = inferred["pwm"]
        self.ana_edits: List[QLineEdit] = []
        self.pwm_edits: List[QLineEdit] = []
        self.in_dig_edits: List[QLineEdit] = []
        self.out_dig_edits: List[QLineEdit] = []
        self.in_freq_edits: List[QLineEdit] = []
        self.enc_abs_edits: List[QLineEdit] = []
        self.enc_rel_edits: List[QLineEdit] = []
        self.enc_speed_edits: List[QLineEdit] = []

        self.host_edit = QLineEdit(host)
        self.port_edit = QLineEdit(str(port))
        self.tick_label = QLabel("0")
        self.auto_refresh_cb = QCheckBox("Auto refresh")
        self.auto_refresh_cb.setChecked(True)

        self.can_node_edit = QLineEdit("0")
        self.can_id_edit = QLineEdit("0x222")
        self.can_data_edit = QLineEdit("1 1 1 1 1 1 1 1")

        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)

        self._build_ui()

    def _build_ui(self) -> None:
        root = QWidget()
        self.setCentralWidget(root)
        root_layout = QVBoxLayout(root)

        top = QHBoxLayout()
        top.addWidget(QLabel("Host"))
        top.addWidget(self.host_edit)
        top.addWidget(QLabel("Port"))
        top.addWidget(self.port_edit)
        connect_btn = QPushButton("Apply")
        connect_btn.clicked.connect(self._reconnect_client)
        top.addWidget(connect_btn)
        ping_btn = QPushButton("Ping")
        ping_btn.clicked.connect(self._ping)
        top.addWidget(ping_btn)
        refresh_btn = QPushButton("Refresh")
        refresh_btn.clicked.connect(self._refresh_once)
        top.addWidget(refresh_btn)
        top.addWidget(self.auto_refresh_cb)
        top.addWidget(QLabel("Tick"))
        top.addWidget(self.tick_label)
        top.addStretch(1)
        root_layout.addLayout(top)

        splitter = QSplitter()
        splitter.addWidget(self._build_ana_group())
        splitter.addWidget(self._build_pwm_group())
        splitter.setSizes([600, 600])
        root_layout.addWidget(splitter, 1)

        splitter_io = QSplitter()
        splitter_io.addWidget(self._build_in_dig_group())
        splitter_io.addWidget(self._build_out_dig_group())
        splitter_io.addWidget(self._build_in_freq_group())
        splitter_io.addWidget(self._build_encoder_group())
        splitter_io.setSizes([300, 300, 300, 500])
        root_layout.addWidget(splitter_io, 1)

        can_group = QGroupBox("CAN Injection")
        can_layout = QHBoxLayout(can_group)
        can_layout.addWidget(QLabel("Node"))
        can_layout.addWidget(self.can_node_edit)
        can_layout.addWidget(QLabel("CAN ID"))
        can_layout.addWidget(self.can_id_edit)
        can_layout.addWidget(QLabel("Bytes"))
        can_layout.addWidget(self.can_data_edit, 1)
        inject_btn = QPushButton("Inject")
        inject_btn.clicked.connect(self._inject_can)
        can_layout.addWidget(inject_btn)
        root_layout.addWidget(can_group)

        log_group = QGroupBox("Log")
        log_layout = QVBoxLayout(log_group)
        log_layout.addWidget(self.log_text)
        root_layout.addWidget(log_group, 1)

    def _build_ana_group(self) -> QWidget:
        container = QGroupBox("Analog Inputs (SET/GET)")
        layout = QGridLayout(container)
        for i in range(self.counts["ana"]):
            label = QLabel(f"ANA[{i}]")
            edit = QLineEdit("0.0")
            set_btn = QPushButton("Set")
            get_btn = QPushButton("Get")
            set_btn.clicked.connect(lambda _=False, idx=i: self._set_ana(idx))
            get_btn.clicked.connect(lambda _=False, idx=i: self._get_ana(idx))
            layout.addWidget(label, i, 0)
            layout.addWidget(edit, i, 1)
            layout.addWidget(set_btn, i, 2)
            layout.addWidget(get_btn, i, 3)
            self.ana_edits.append(edit)
        return self._wrap_scroll(container)

    def _build_pwm_group(self) -> QWidget:
        container = QGroupBox("PWM Outputs (SET/GET)")
        layout = QGridLayout(container)
        for i in range(self.counts["pwm"]):
            label = QLabel(f"PWM[{i}]")
            edit = QLineEdit("0")
            set_btn = QPushButton("Set")
            get_btn = QPushButton("Get")
            set_btn.clicked.connect(lambda _=False, idx=i: self._set_pwm(idx))
            get_btn.clicked.connect(lambda _=False, idx=i: self._get_pwm(idx))
            layout.addWidget(label, i, 0)
            layout.addWidget(edit, i, 1)
            layout.addWidget(set_btn, i, 2)
            layout.addWidget(get_btn, i, 3)
            self.pwm_edits.append(edit)
        return self._wrap_scroll(container)

    def _build_in_dig_group(self) -> QWidget:
        container = QGroupBox("Input Digital (SET/GET)")
        layout = QGridLayout(container)
        if self.counts["in_dig"] == 0:
            layout.addWidget(QLabel("No Input Digital configured in FMKIO_ConfigPublic.h"), 0, 0)
            return self._wrap_scroll(container)

        for i in range(self.counts["in_dig"]):
            label = QLabel(f"IN_DIG[{i}]")
            edit = QLineEdit("0")
            set_btn = QPushButton("Set")
            get_btn = QPushButton("Get")
            set_btn.clicked.connect(lambda _=False, idx=i: self._set_in_dig(idx))
            get_btn.clicked.connect(lambda _=False, idx=i: self._get_in_dig(idx))
            layout.addWidget(label, i, 0)
            layout.addWidget(edit, i, 1)
            layout.addWidget(set_btn, i, 2)
            layout.addWidget(get_btn, i, 3)
            self.in_dig_edits.append(edit)
        return self._wrap_scroll(container)

    def _build_in_freq_group(self) -> QWidget:
        container = QGroupBox("Input Frequency (SET/GET)")
        layout = QGridLayout(container)
        if self.counts["in_freq"] == 0:
            layout.addWidget(QLabel("No Input Frequency configured in FMKIO_ConfigPublic.h"), 0, 0)
            return self._wrap_scroll(container)

        for i in range(self.counts["in_freq"]):
            label = QLabel(f"IN_FREQ[{i}]")
            edit = QLineEdit("0.0")
            set_btn = QPushButton("Set")
            get_btn = QPushButton("Get")
            set_btn.clicked.connect(lambda _=False, idx=i: self._set_in_freq(idx))
            get_btn.clicked.connect(lambda _=False, idx=i: self._get_in_freq(idx))
            layout.addWidget(label, i, 0)
            layout.addWidget(edit, i, 1)
            layout.addWidget(set_btn, i, 2)
            layout.addWidget(get_btn, i, 3)
            self.in_freq_edits.append(edit)
        return self._wrap_scroll(container)

    def _build_out_dig_group(self) -> QWidget:
        container = QGroupBox("Output Digital (SET/GET)")
        layout = QGridLayout(container)
        if self.counts["out_dig"] == 0:
            layout.addWidget(QLabel("No Output Digital configured in FMKIO_ConfigPublic.h"), 0, 0)
            return self._wrap_scroll(container)

        for i in range(self.counts["out_dig"]):
            label = QLabel(f"OUT_DIG[{i}]")
            edit = QLineEdit("0")
            set_btn = QPushButton("Set")
            get_btn = QPushButton("Get")
            set_btn.clicked.connect(lambda _=False, idx=i: self._set_out_dig(idx))
            get_btn.clicked.connect(lambda _=False, idx=i: self._get_out_dig(idx))
            layout.addWidget(label, i, 0)
            layout.addWidget(edit, i, 1)
            layout.addWidget(set_btn, i, 2)
            layout.addWidget(get_btn, i, 3)
            self.out_dig_edits.append(edit)
        return self._wrap_scroll(container)

    def _build_encoder_group(self) -> QWidget:
        container = QGroupBox("Encoder (SET/GET)")
        layout = QGridLayout(container)
        if self.counts["enc"] == 0:
            layout.addWidget(QLabel("No Encoder configured in FMKIO_ConfigPublic.h"), 0, 0)
            return self._wrap_scroll(container)

        for i in range(self.counts["enc"]):
            base_row = i * 2
            layout.addWidget(QLabel(f"ENC[{i}] abs/rel"), base_row, 0)
            abs_edit = QLineEdit("0.0")
            rel_edit = QLineEdit("0.0")
            set_pos_btn = QPushButton("Set Pos")
            get_pos_btn = QPushButton("Get Pos")
            set_pos_btn.clicked.connect(lambda _=False, idx=i: self._set_enc_pos(idx))
            get_pos_btn.clicked.connect(lambda _=False, idx=i: self._get_enc_pos(idx))
            layout.addWidget(abs_edit, base_row, 1)
            layout.addWidget(rel_edit, base_row, 2)
            layout.addWidget(set_pos_btn, base_row, 3)
            layout.addWidget(get_pos_btn, base_row, 4)

            layout.addWidget(QLabel("speed"), base_row + 1, 0)
            speed_edit = QLineEdit("0.0")
            set_speed_btn = QPushButton("Set Speed")
            get_speed_btn = QPushButton("Get Speed")
            set_speed_btn.clicked.connect(lambda _=False, idx=i: self._set_enc_speed(idx))
            get_speed_btn.clicked.connect(lambda _=False, idx=i: self._get_enc_speed(idx))
            layout.addWidget(speed_edit, base_row + 1, 1)
            layout.addWidget(set_speed_btn, base_row + 1, 3)
            layout.addWidget(get_speed_btn, base_row + 1, 4)

            self.enc_abs_edits.append(abs_edit)
            self.enc_rel_edits.append(rel_edit)
            self.enc_speed_edits.append(speed_edit)
        return self._wrap_scroll(container)

    @staticmethod
    def _wrap_scroll(widget: QWidget) -> QScrollArea:
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setWidget(widget)
        return scroll

    def _log(self, message: str) -> None:
        self.log_text.append(message)

    def _show_error(self, title: str, exc: Exception) -> None:
        self._log(f"[ERR] {title}: {exc}")
        QMessageBox.critical(self, title, str(exc))

    def _reconnect_client(self) -> None:
        try:
            host = self.host_edit.text().strip() or "127.0.0.1"
            port = int(self.port_edit.text().strip(), 0)
            self.client = PcSimClient(host=host, port=port)
            self._log(f"[INFO] client set to {host}:{port}")
        except Exception as exc:
            self._show_error("Apply", exc)

    def _ping(self) -> None:
        try:
            rsp = self.client.ping()
            self._log(rsp)
        except Exception as exc:
            self._show_error("Ping", exc)

    def _set_ana(self, idx: int) -> None:
        try:
            value = float(self.ana_edits[idx].text())
            self.client.set_ana(idx, value)
        except Exception as exc:
            self._show_error("SET_ANA", exc)

    def _get_ana(self, idx: int) -> None:
        try:
            val = self.client.get_ana(idx)
            self.ana_edits[idx].setText(f"{val}")
        except Exception as exc:
            self._show_error("GET_ANA", exc)

    def _set_pwm(self, idx: int) -> None:
        try:
            duty = int(self.pwm_edits[idx].text(), 0)
            self.client.set_pwm(idx, duty)
        except Exception as exc:
            self._show_error("SET_PWM", exc)

    def _get_pwm(self, idx: int) -> None:
        try:
            val = self.client.get_pwm(idx)
            self.pwm_edits[idx].setText(f"{val}")
        except Exception as exc:
            self._show_error("GET_PWM", exc)

    def _inject_can(self) -> None:
        try:
            node = int(self.can_node_edit.text().strip(), 0)
            can_id = int(self.can_id_edit.text().strip(), 0)
            data = [int(x, 0) for x in self.can_data_edit.text().split() if x.strip()]
            self.client.inject_can(node, can_id, data)
            self._log(f"[CAN] injected node={node} id=0x{can_id:X} data={data}")
        except Exception as exc:
            self._show_error("INJECT_CAN", exc)

    def _set_in_dig(self, idx: int) -> None:
        try:
            value = int(self.in_dig_edits[idx].text(), 0)
            self.client.set_in_dig(idx, value)
        except Exception as exc:
            self._show_error("SET_IN_DIG", exc)

    def _get_in_dig(self, idx: int) -> None:
        try:
            value = self.client.get_in_dig(idx)
            self.in_dig_edits[idx].setText(str(value))
        except Exception as exc:
            self._show_error("GET_IN_DIG", exc)

    def _set_in_freq(self, idx: int) -> None:
        try:
            value = float(self.in_freq_edits[idx].text())
            self.client.set_in_freq(idx, value)
        except Exception as exc:
            self._show_error("SET_IN_FREQ", exc)

    def _get_in_freq(self, idx: int) -> None:
        try:
            value = self.client.get_in_freq(idx)
            self.in_freq_edits[idx].setText(f"{value}")
        except Exception as exc:
            self._show_error("GET_IN_FREQ", exc)

    def _set_out_dig(self, idx: int) -> None:
        try:
            value = int(self.out_dig_edits[idx].text(), 0)
            self.client.set_out_dig(idx, value)
        except Exception as exc:
            self._show_error("SET_OUT_DIG", exc)

    def _get_out_dig(self, idx: int) -> None:
        try:
            value = self.client.get_out_dig(idx)
            self.out_dig_edits[idx].setText(str(value))
        except Exception as exc:
            self._show_error("GET_OUT_DIG", exc)

    def _set_enc_pos(self, idx: int) -> None:
        try:
            abs_val = float(self.enc_abs_edits[idx].text())
            rel_val = float(self.enc_rel_edits[idx].text())
            self.client.set_enc_pos(idx, abs_val, rel_val)
        except Exception as exc:
            self._show_error("SET_ENC_POS", exc)

    def _get_enc_pos(self, idx: int) -> None:
        try:
            abs_val, rel_val = self.client.get_enc_pos(idx)
            self.enc_abs_edits[idx].setText(f"{abs_val}")
            self.enc_rel_edits[idx].setText(f"{rel_val}")
        except Exception as exc:
            self._show_error("GET_ENC_POS", exc)

    def _set_enc_speed(self, idx: int) -> None:
        try:
            speed = float(self.enc_speed_edits[idx].text())
            self.client.set_enc_speed(idx, speed)
        except Exception as exc:
            self._show_error("SET_ENC_SPEED", exc)

    def _get_enc_speed(self, idx: int) -> None:
        try:
            speed = self.client.get_enc_speed(idx)
            self.enc_speed_edits[idx].setText(f"{speed}")
        except Exception as exc:
            self._show_error("GET_ENC_SPEED", exc)

    def _refresh_once(self) -> None:
        if not self.auto_refresh_cb.isChecked():
            return
        try:
            rsp = self.client.get_all()
            parts = rsp.split()
            tick_idx = parts.index("TICK")
            ana_idx = parts.index("ANA")
            pwm_idx = parts.index("PWM")
            self.tick_label.setText(parts[tick_idx + 1])
            ana_vals = parts[ana_idx + 1 : pwm_idx]
            pwm_vals = parts[pwm_idx + 1 :]
            for i in range(min(self.counts["ana"], len(ana_vals))):
                self.ana_edits[i].setText(ana_vals[i])
            for i in range(min(self.counts["pwm"], len(pwm_vals))):
                self.pwm_edits[i].setText(pwm_vals[i])
        except (TimeoutError, socket.timeout):
            self._log("[WARN] refresh timeout")
        except Exception:
            # Keep periodic refresh non-intrusive.
            pass


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=19090)
    args = parser.parse_args()

    app = QApplication(sys.argv)
    win = PcSimIhmWindow(host=args.host, port=args.port)
    win.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
