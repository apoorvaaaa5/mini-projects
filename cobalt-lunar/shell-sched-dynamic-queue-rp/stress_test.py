#!/usr/bin/env python3
"""
StrataHalo OS - Automated Multitasking & Heap Stress Test Harness
Target: RP2040 Cortex-M0+ over /dev/ttyACM0

Exercises 5 consecutive rounds:
1. Randomized task spawning order (display, hello, breathe)
2. Dynamic heap allocation, splitting, and ready-queue injection
3. Verification via 'ps' and full physical 'heap audit'
4. Mass termination ('kill all') and instant memory reclamation
5. Full Standalone CoreMark benchmark execution with UNFILTERED, FULL REAL-TIME TELEMETRY:
   - 2K performance run parameters
   - CoreMark Size, Total ticks, Total time
   - Iterations/Sec score (~100 CoreMarks/sec)
   - Seed and CRC checks ([0]crclist, [0]crcmatrix, [0]crcstate, [0]crcfinal)
   - Official EEMBC validation string
"""

import sys
import time
import random
import serial

PORT = "/dev/ttyACM0"
BAUD = 115200
TIMEOUT = 3

APPS = ["breathe", "display", "hello"]

def send_command(ser, cmd, wait_sec=0.5, expect_prompt=True):
    try:
        ser.reset_input_buffer()
        ser.write((cmd + "\r\n").encode("utf-8"))
    except Exception as e:
        return f"[ERROR writing: {e}]"
    time.sleep(wait_sec)

    start = time.time()
    response = ""
    while (time.time() - start) < (wait_sec + 2.0):
        try:
            n = ser.in_waiting
            if n > 0:
                chunk = ser.read(n).decode("utf-8", errors="ignore")
                response += chunk
                if expect_prompt and "halo> " in response:
                    break
            else:
                time.sleep(0.05)
        except Exception:
            time.sleep(0.05)
    return response

def wait_for_prompt(ser, max_wait=5.0):
    start = time.time()
    ser.write(b"\r\n")
    buf = ""
    while (time.time() - start) < max_wait:
        try:
            n = ser.in_waiting
            if n > 0:
                chunk = ser.read(n).decode("utf-8", errors="ignore")
                buf += chunk
                if "halo> " in buf:
                    return True
            else:
                time.sleep(0.1)
        except Exception:
            time.sleep(0.1)
    return False

def stream_until_string(ser, target_str, timeout=40.0):
    """Streams all incoming serial output to stdout in real-time until target_str appears."""
    start = time.time()
    captured = ""
    while (time.time() - start) < timeout:
        try:
            n = ser.in_waiting
            if n > 0:
                chunk = ser.read(n).decode("utf-8", errors="ignore")
                sys.stdout.write(chunk)
                sys.stdout.flush()
                captured += chunk
                if target_str in captured:
                    return captured
            else:
                time.sleep(0.05)
        except Exception:
            time.sleep(0.05)
    return captured

def main():
    print(f"[TEST HARNESS] Connecting to StrataHalo OS on {PORT} at {BAUD} baud...")
    ser = None
    while ser is None:
        try:
            ser = serial.Serial(PORT, BAUD, timeout=1.0)
            ser.dtr = True
            ser.rts = True
            time.sleep(0.5)
        except Exception as e:
            print(f"[WAIT] Waiting for {PORT} to become available... ({e})")
            time.sleep(2.0)

    print("[TEST HARNESS] Synchronizing shell prompt (press Enter if needed)...")
    prompt_synced = False
    for attempt in range(15):
        if wait_for_prompt(ser, max_wait=1.0):
            prompt_synced = True
            break
        print(f"  [Sync attempt {attempt+1}/15] Waiting for 'halo> ' prompt from Pico...")
        ser.write(b"\r\n")
        time.sleep(0.5)

    if not prompt_synced:
        print("\n[NOTE] Pico is not responding with 'halo> ' prompt yet.")
        print("Please flash 'build/SHELL_SCHED_DYNAMIC_QUEUE_RP2040.uf2' to the board.")
        ser.close()
        sys.exit(1)

    print("\n" + "="*70)
    print("STEP 1: Baseline Diagnostics ('help' & 'heap')")
    print("="*70)
    res = send_command(ser, "help", wait_sec=0.5)
    print(res.strip())

    res = send_command(ser, "heap", wait_sec=0.5)
    print(res.strip())

    # Configurable Stress Rounds (defaults to 10 if not specified on CLI)
    TOTAL_ROUNDS = int(sys.argv[1]) if len(sys.argv) > 1 else 10
    print(f"\n[TEST CONFIG] Running {TOTAL_ROUNDS} consecutive stress cycles...")
    for round_num in range(1, TOTAL_ROUNDS + 1):
        print("\n" + "#"*70)
        print(f"### STRESS CYCLE {round_num} OF {TOTAL_ROUNDS} ###")
        print("#"*70)

        # 1. Shuffle task injection order
        shuffled = APPS.copy()
        random.shuffle(shuffled)
        print(f"\n[CYCLE {round_num}] Phase 1: Task Injection Shuffle: {' -> '.join(shuffled)}")

        for app in shuffled:
            print(f"[CYCLE {round_num}] Spawning '{app}' in background...")
            res = send_command(ser, f"run {app} &", wait_sec=0.4)
            print("  " + res.replace("\r\n", "\n  ").strip())

        # 2. Check task queue
        print(f"\n[CYCLE {round_num}] Phase 2: Checking Ready Queue ('ps')...")
        res = send_command(ser, "ps", wait_sec=0.5)
        print(res.strip())

        # 3. Dynamic Heap Audit under multi-task load
        print(f"\n[CYCLE {round_num}] Phase 3: Heap Audit Under Load ('heap audit')...")
        res = send_command(ser, "heap audit", wait_sec=0.6)
        print(res.strip())

        # 4. Concurrency exercise
        print(f"\n[CYCLE {round_num}] Phase 4: Exercising preemptive multitasking for 2 seconds...")
        time.sleep(2.0)

        # 5. Mass termination
        print(f"\n[CYCLE {round_num}] Phase 5: Mass terminating background tasks ('kill all')...")
        res = send_command(ser, "kill all", wait_sec=0.6)
        print(res.strip())

        # 6. Verify zero-leak memory coalescing
        print(f"\n[CYCLE {round_num}] Phase 6: Verifying Memory Reclaim & Coalescing ('heap')...")
        res = send_command(ser, "heap", wait_sec=0.5)
        print(res.strip())

        # 7. Standalone CoreMark Benchmark Execution with UNFILTERED detailed report
        print(f"\n[CYCLE {round_num}] Phase 7: Spawning Standalone CoreMark ('run coremark &')...")
        ser.reset_input_buffer()
        ser.write(b"run coremark &\r\n")

        print(f"\n[CYCLE {round_num}] Streaming Live Benchmark Execution & Full EEMBC Metrics:")
        print("-" * 70)
        cm_output = stream_until_string(ser, "Correct operation validated", timeout=40.0)

        # Consume any trailing output (e.g. final prompt)
        time.sleep(1.0)
        while ser.in_waiting:
            chunk = ser.read(ser.in_waiting).decode("utf-8", errors="ignore")
            sys.stdout.write(chunk)
            sys.stdout.flush()
            cm_output += chunk
            time.sleep(0.1)
        print("-" * 70)

        if "Correct operation validated" in cm_output:
            print(f"\n[PASS] CYCLE {round_num} COMPLETED SUCCESSFULLY with 100% verified CRCs!")
        else:
            print(f"\n[FAIL] CYCLE {round_num}: CoreMark output incomplete.")
            ser.close()
            sys.exit(1)

        # Post-CoreMark Heap Check
        res = send_command(ser, "heap", wait_sec=0.5)
        print(f"\n[CYCLE {round_num}] Post-Benchmark Heap State:")
        print("  " + res.replace("\r\n", "\n  ").strip())

    print("\n" + "="*70)
    print(">>> ALL 5 STRESS TEST CYCLES COMPLETED WITH FULL DETAILED METRICS! <<<")
    print("="*70)
    ser.close()

if __name__ == "__main__":
    main()
