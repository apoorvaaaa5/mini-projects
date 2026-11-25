#!/usr/bin/env python3
import serial
import time
import sys
import re

PORT = "/dev/ttyACM0"
BAUD = 115200
TOTAL_CYCLES = 20

if len(sys.argv) > 1:
    try:
        TOTAL_CYCLES = int(sys.argv[1])
    except ValueError:
        pass

print(f"[TEST HARNESS] Connecting to StrataHalo OS on {PORT} at {BAUD} baud...")
try:
    ser = serial.Serial(PORT, BAUD, timeout=1.0)
except Exception as e:
    print(f"[FATAL] Cannot open {PORT}: {e}")
    sys.exit(1)

def send_line(cmd, wait_s=0.2):
    ser.write(f"{cmd}\r\n".encode("utf-8"))
    time.sleep(wait_s)

def drain_to_string(timeout=2.0):
    start = time.time()
    buf = ""
    while (time.time() - start) < timeout:
        chunk = ser.read(ser.in_waiting or 1).decode("utf-8", errors="ignore")
        if chunk:
            buf += chunk
            start = time.time()
        else:
            time.sleep(0.05)
    return buf

# Synchronize shell
send_line("")
drain_to_string(0.5)

print("\n" + "=" * 70)
print(f"STEP 1: Checking Baseline System State ('ps' & 'heap')")
print("=" * 70)
send_line("ps")
print(drain_to_string(0.8))

send_line("heap")
print(drain_to_string(0.8))

print(f"\n[CONFIG] Launching {TOTAL_CYCLES} Concurrent Multitasking Stress Cycles...")
print("Every cycle will have ALL tasks ('display', 'breathe', 'coremark') running CONCURRENTLY!\n")

results_summary = []

for cycle in range(1, TOTAL_CYCLES + 1):
    print(f"\n{'#' * 70}")
    print(f"### CONCURRENT STRESS CYCLE {cycle} OF {TOTAL_CYCLES} ###")
    print(f"{'#' * 70}\n")

    # 1. Clean slate for cycle if anything was left
    send_line("kill all", 0.3)
    drain_to_string(0.5)

    # 2. Spawn continuous background daemons
    print(f"[CYCLE {cycle}] Spawning 'breathe' (PWM LED daemon) in background...")
    send_line("run breathe &", 0.3)
    time.sleep(0.2)

    print(f"[CYCLE {cycle}] Spawning 'display' (interactive spinner) in background...")
    send_line("run display &", 0.3)
    time.sleep(0.3)

    # 3. Spawn CoreMark concurrently with the others running!
    print(f"[CYCLE {cycle}] Spawning 'coremark' CONCURRENTLY with all background tasks running...")
    send_line("run coremark &", 0.4)
    time.sleep(0.5)

    # 4. Check Ready Queue ('ps') to verify all tasks are simultaneously active
    print(f"\n[CYCLE {cycle}] Verifying Dynamic Ready Queue ('ps'):")
    send_line("ps", 0.3)
    ps_output = drain_to_string(0.8)
    print(ps_output)

    # Check that all tasks are present
    has_shell = "shell" in ps_output
    has_breathe = "breathe" in ps_output
    has_display = "display" in ps_output
    has_coremark = "coremark" in ps_output

    if not (has_breathe and has_display and has_coremark):
        print(f"[WARNING] Not all tasks running concurrently in cycle {cycle}!")

    # 5. Heap audit while ALL tasks are executing simultaneously
    print(f"\n[CYCLE {cycle}] Inspecting Dynamic Heap Under Full Concurrent Load ('heap'):")
    send_line("heap", 0.3)
    print(drain_to_string(0.8))

    # 6. Stream execution until CoreMark finishes
    print(f"\n[CYCLE {cycle}] Monitoring CoreMark Execution Under Preemptive Concurrency:")
    print("-" * 70)
    
    benchmark_output = ""
    start_time = time.time()
    coremark_done = False
    
    # CoreMark running concurrently with 2 other busy tasks takes ~40-70 seconds
    max_wait = 120.0
    while (time.time() - start_time) < max_wait:
        chunk = ser.read(ser.in_waiting or 1).decode("utf-8", errors="ignore")
        if chunk:
            sys.stdout.write(chunk)
            sys.stdout.flush()
            benchmark_output += chunk
            if "Correct operation validated" in chunk or "[OK] CoreMark Benchmark run completed" in chunk:
                # Allow trailing output
                time.sleep(1.0)
                trailing = ser.read(ser.in_waiting or 1).decode("utf-8", errors="ignore")
                sys.stdout.write(trailing)
                sys.stdout.flush()
                benchmark_output += trailing
                coremark_done = True
                break
        else:
            time.sleep(0.05)

    print("-" * 70)

    # 7. Extract metrics
    score_match = re.search(r"Iterations/Sec\s*:\s*([\d\.]+)", benchmark_output)
    ticks_match = re.search(r"Total ticks\s*:\s*(\d+)", benchmark_output)
    validated = "Correct operation validated" in benchmark_output

    score_val = score_match.group(1) if score_match else "N/A"
    ticks_val = ticks_match.group(1) if ticks_match else "N/A"

    if coremark_done and validated:
        print(f"\n[PASS] CYCLE {cycle} COMPLETED SUCCESSFULLY!")
        print(f"       Concurrent Score: {score_val} Iterations/Sec | Wall Ticks: {ticks_val} ms | CRCs Validated: YES")
        results_summary.append({
            "cycle": cycle,
            "status": "PASS",
            "score": score_val,
            "ticks": ticks_val,
            "concurrent": "breathe + display + coremark"
        })
    else:
        print(f"\n[FAIL] CYCLE {cycle} Did not validate or timed out!")
        results_summary.append({
            "cycle": cycle,
            "status": "FAIL",
            "score": score_val,
            "ticks": ticks_val,
            "concurrent": "breathe + display + coremark"
        })

    # 8. Verify post-benchmark heap
    send_line("heap", 0.3)
    post_heap = drain_to_string(0.8)
    print(f"\n[CYCLE {cycle}] Post-Benchmark Heap State:")
    print(post_heap)

    # 9. Clean up tasks before next cycle
    send_line("kill all", 0.3)
    drain_to_string(0.5)

print("\n" + "=" * 70)
print(f"SUMMARY OF ALL {TOTAL_CYCLES} CONCURRENT MULTITASKING TESTS")
print("=" * 70)
for r in results_summary:
    print(f"Cycle {r['cycle']:02d}: [{r['status']}] Tasks: {r['concurrent']} | Score: {r['score']} Iter/s | Wall Ticks: {r['ticks']} ms")

ser.close()
