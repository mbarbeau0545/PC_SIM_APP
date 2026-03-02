# PC_SIM - Simulation applicative `3_APP`

Ce dossier fournit une ex�cution PC de la couche application (`src/3_APP`) avec un framework FMK simul�.

## Architecture

Le code de simulation est dans `src/4_PCSIM` et est s�par� par th�me:

- `pc_sim_runtime.c/.h`: runtime commun (temps, �tats modules, stockage I/O, timer fast task)
- `pc_sim_fmk_cpu.c`: stubs CPU
- `pc_sim_fmk_tim.c`: stubs TIM
- `pc_sim_fmk_hrt.c`: stubs HRT
- `pc_sim_fmk_cda.c`: stubs CDA
- `pc_sim_fmk_io.c`: stubs I/O + API PC `SET/GET` (analog, digital, freq, PWM, encodeur)
- `pc_sim_fmk_can.c`: stubs CAN + injection RX + capture des trames TX envoy�es par l'app
- `pc_sim_fmk_srl.c`: stubs SRL (logs/tx redirig�s sur la console terminal)
- `main_pc_sim.c`: boucle principale + serveur UDP de commande

## Build

```bash
pio run -e pc_sim
```

Binaire produit:

- `.pio/build/pc_sim/program.exe`

## Lancement

```bash
.pio/build/pc_sim/program.exe
```

Options disponibles:

- `--udp-port <port>`: port UDP serveur (d�faut `19090`)
- `--sleep-ms <ms>`: pause de boucle principale (d�faut `1`)
- `--ana <id> <value>`: initialise une entr�e analogique au d�marrage (option r�p�table)
- `--help`: affiche l'aide

Exemple:

```bash
.pio/build/pc_sim/program.exe --udp-port 20001 --sleep-ms 2 --ana 1 3000 --ana 3 1200
```

## Debugger

### Build debug

```bash
pio run -e pc_sim -t clean
pio run -e pc_sim --project-option="build_type=debug"
```

### Debug GDB (terminal)

```bash
gdb .pio/build/pc_sim/program.exe
```

Dans gdb:

```gdb
break main
run --udp-port 19090
```

### VS Code

Lancer `program.exe` depuis la configuration C/C++ Debug (type gdb/lldb) avec les arguments de ton choix (`--udp-port`, `--ana`, etc.).

## Protocole UDP (127.0.0.1)

Le serveur �coute sur `127.0.0.1:<udp-port>`.

Commandes:

- `PING`
- `HELP`
- `GET_ALL`
- `SET_ANA <id> <value_mv>`
- `GET_ANA <id>`
- `SET_PWM <id> <duty_0_1000>`
- `GET_PWM <id>`
- `SET_PWM_FREQ <id> <freq_hz>`
- `GET_PWM_FREQ <id>`
- `SET_PWM_PULSES <id> <pulses>`
- `GET_PWM_PULSES <id>`
- `SET_IN_DIG <id> <0|1>`
- `GET_IN_DIG <id>`
- `SET_IN_FREQ <id> <freq_hz>`
- `GET_IN_FREQ <id>`
- `SET_ENC_POS <id> <abs> <rel>`
- `GET_ENC_POS <id>`
- `SET_ENC_SPEED <id> <speed>`
- `GET_ENC_SPEED <id>`
- `INJECT_CAN <node> <can_id> <dlc> <b0> .. <bN>`
- `GET_CAN_TX_COUNT`
- `POP_CAN_TX`
- `CLEAR_CAN_TX`

Exemple:

```text
INJECT_CAN 0 0x1810A57B 8 1 1 1 1 1 1 1 1
```

## IHM Python

```bash
python tools/pc_sim/pc_sim_ihm.py
```

Fonctionnalités principales:

- Onglet `I/O`: set/get analog, PWM, digital in/out, freq in, encodeurs.
- Onglet `CAN`: injection CAN RX + visualisation des trames CAN TX émises par la simulation STM32.
- Onglet `Encoders`: modes de simulation par encodeur:
`manual`, `ramp`, `constant_speed`, `trapezoidal`, `sinusoidal`, `script`, `pulse_based`.
- Défauts/robustesse encodeur: freeze, inversion speed, spikes, offset jump, bruit, quantification, jitter.

Persistance configuration encodeurs:

- Fichier: `tools/pc_sim/pc_sim_encoder_config.json`
- Chargé automatiquement au démarrage de `pc_sim_ihm.py`
- Sauvegardé via bouton `Save Config` et à la fermeture de l'IHM

## Client Python

```bash
python tools/pc_sim/can_sender_example.py
```

Le module `pc_sim_client.py` expose une API simple (`set_ana`, `set_pwm`, `inject_can`, etc.) pour scripts de test.

Nouvelles API Python disponibles:

- `set_pwm_freq` / `get_pwm_freq`
- `set_pwm_pulses` / `get_pwm_pulses`
- `set_in_dig` / `get_in_dig`
- `set_in_freq` / `get_in_freq`
- `set_enc_pos` / `get_enc_pos`
- `set_enc_speed` / `get_enc_speed`
- `get_can_tx_count` / `pop_can_tx` / `clear_can_tx`
## Monitor Python live

```bash
python tools/pc_sim/pc_sim_monitor.py --port 19090 --stimulate
```

Options utiles:

- `--period-ms 200`: p�riode d'acquisition/commande
- `--duration-s 10`: arr�t automatique apr�s N secondes
- `--clear-can-tx-on-start`: purge le buffer CAN TX au d�marrage
- `--max-can-pop 100`: limite de trames CAN affich�es par cycle
- `--in-dig-idx`, `--in-freq-idx`, `--enc-idx`, `--pwm-idx`: indices des signaux test�s
