import time

from pc_sim_client import PcSimClient


if __name__ == "__main__":
    cli = PcSimClient()
    print(cli.ping())

    # Example: set all detected analog inputs to mid values (based on GET_ALL).
    ana_count = 0
    try:
        parts = cli.get_all().split()
        if "ANA" in parts and "PWM" in parts:
            ana_idx = parts.index("ANA")
            pwm_idx = parts.index("PWM")
            ana_count = max(0, pwm_idx - ana_idx - 1)
    except Exception:
        ana_count = 0

    for ana_id in range(ana_count):
        cli.set_ana(ana_id, 2500.0)

    # Example: periodically inject CAN frame for power relay states
    # format: node id dlc b0..b7
    while True:
        rsp = cli.inject_can(node=0, can_id=0x18FFA57B, payload=[1, 1, 1, 1, 1, 1, 1, 1])
        print(rsp)
        time.sleep(0.2)
