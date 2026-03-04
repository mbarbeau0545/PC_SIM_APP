# PC_SIM - Simulation applicative `3_APP`

PC_SIM execute l'application STM32 sur PC avec stubs FMK et un serveur UDP local.

## Build

```bash
pio run -e pc_sim_debug
```

Binaire:

- `.pio/build/pc_sim_debug/program.exe`

## Lancement

```bash
.pio/build/pc_sim_debug/program.exe --udp-port 19090 --sleep-ms 1
```

Options:

- `--udp-port <port>`
- `--sleep-ms <ms>`
- `--ana <id> <value>` (repeatable)
- `--enc-map <enc_idx> <pwm_idx> <pulses_per_rev> [dir_dig_idx]` (repeatable)

## Protocole UDP

Serveur sur `127.0.0.1:<udp-port>`.

### I/O

- `PING`, `HELP`, `GET_ALL`
- `SET_ANA/GET_ANA`
- `SET_PWM/GET_PWM`
- `SET_PWM_FREQ/GET_PWM_FREQ`
- `SET_PWM_PULSES/GET_PWM_PULSES`
- `SET_IN_DIG/GET_IN_DIG`
- `SET_OUT_DIG/GET_OUT_DIG`
- `SET_IN_FREQ/GET_IN_FREQ`
- `SET_ENC_POS/GET_ENC_POS`
- `SET_ENC_SPEED/GET_ENC_SPEED`
- `SET_ENC_MAP <enc_idx> <pwm_idx> <pulses_per_rev> [dir_dig_idx]`
  - `dir_dig_idx` optionnel: `OUT_DIG=0` => sens negatif (encoder diminue), `OUT_DIG=1` => sens positif (encoder augmente).

### CAN

Injection RX:

- `INJECT_CAN <node> <can_id> <dlc> <b0>..<bN>` (legacy, extended)
- `INJECT_CAN_EX <node> <can_id> <ext0or1> <dlc> <b0>..<bN>`

Queue CAN TX IHM (historique):

- `GET_CAN_TX_COUNT`
- `POP_CAN_TX`
- `POP_CAN_TX_BURST <n>`
- `CLEAR_CAN_TX`

Queue CAN TX dediee broker (nouveau):

- `GET_CAN_BROKER_TX_COUNT`
- `POP_CAN_BROKER_TX_BURST <n>`
- `CLEAR_CAN_BROKER_TX`

Abonnements RX runtime (nouveau):

- `GET_CAN_RX_REG_COUNT`
- `DUMP_CAN_RX_REG_BURST <n>`

Format `DUMP_CAN_RX_REG_BURST`:

- `OK RC 0 REG NODE <node> ID <id> MASK <mask> EXT <0|1> ...`

## Changements architecture CAN

- Les enregistrements RX PCSIM sont maintenant **portes par node CAN**.
- `PCSIM_InjectCanFrame()` n'injecte que vers les callbacks configures sur le node cible.
- Les trames TX sont dupliquees dans 2 files:
  - file IHM (`POP_CAN_TX*`)
  - file broker (`POP_CAN_BROKER_TX*`)

Cela permet un broker central multi-ECU sans perturber l'affichage IHM.

## Demarrage broker dans le flux projet

- `Doc/ConfigPrj/launch_multi_exe.bat` peut demarrer le broker avant les ECU.
- Detection d'instance broker par ping UDP `PING/PONG` sur `general.can_broker.control_port` (default `19600`).
- Si le monitor MultiEcu est deja lance (broker in-process), le `.bat` ne lance pas de second broker.
