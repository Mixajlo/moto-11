# 20. IMU security and crash logic: calibration, MOVED / BUMP / TIPPED, crash with cancel window

- Status: Accepted (first implementation; thresholds to be tuned on the bike)
- Date: 2026-11-10
- Deciders: Miki

## Context
The roadmap calls for a 6-axis IMU that covers theft wake, tilt and crash detection, with
corroboration (orientation and was-moving) and a 30 s cancel window before an SMS. The
mainboard already reserves an MPU-6050 breakout (A2, I²C 0x68, INT → GPIO 33). Bench
measurements on the bike's twin (2026-11-10) shaped the design:

- At rest the noise is **±0.01 g and ±0.1 °/s**. The **z offset is +0.115 g** and the gyro
  bias about 3 °/s, so calibration is mandatory.
- About **1 % of I²C burst reads come back corrupt**, as the pattern `FF 00 FF 00…`. A
  single sample can't be trusted.
- Polling from the PC (about 3 Hz) can't see short knocks. Detection has to run on the ESP
  from the IMU's own 100 Hz output, plus its hardware motion interrupt.
- The bike has **no speed signal** yet: K-line SDS and GPS come later. A crash usually
  stalls the engine (and the factory tip-over sensor cuts it), so "engine RUNNING right now"
  is not enough to mean "was moving".

## Decision
We will add a pure-logic `MotionMonitor` (`src/motion.{h,cpp}`). `main.cpp` feeds it every
IMU sample at up to 100 Hz, along with whether the engine is RUNNING. It is unit-tested on
the host with synthetic motion (`test_host/test_motion_native.cpp`).

- **Calibration** (`imu cal`, with the bike still and upright in its normal parked pose):
  - Average about 2 s of samples and store the gyro bias and the *upright gravity vector* in
    NVS (`motoimu`). The upright vector also absorbs the mounting angle and the accel offset
    in that pose.
  - Reject the run if any axis's **standard deviation** exceeds 0.01 g or 1 °/s. We use the
    standard deviation rather than min–max because one corrupt sample must not fail a still
    calibration.
  - Impact is |a| scaled so that the calibrated rest reads 1.00 g.
- **Context:**
  - *Riding* = RUNNING now, or within the last 10 s (`rideHoldMs`).
  - *Parked* = anything else.
- **Events:**
  - **BUMP** (parked): |a| − 1 g > 0.15 g on 2 consecutive samples; at most one every 5 s.
  - **MOVED** (parked): the low-pass attitude leaves the *parked reference* by more than
    `moveDeg` (3°) for 1 s. The reference is taken after 10 s still, and re-taken after
    10 s still at a new attitude, so a bike moved and left stays armed.
  - **TIPPED / UPRIGHT** (any context): more than `tipDeg` (60°) from upright for 2 s;
    upright again below 50° for 2 s. A side stand is about 10–15°.
  - **CRASH** (riding only):
    1. an impact above `crashG` (3 g, 2 consecutive samples);
    2. then lying down (> `tipDeg`) within 3 s, held for 3 s, gives **PENDING**;
    3. after `crashCancel` (30 s) it becomes **CONFIRMED**. This is where the SMS goes, via
       the tablet's LTE.

    The alert is cancelled by `crash cancel` (later a button or the tablet), or
    automatically when the bike stands upright again for 2 s. An impact without a fall
    (a pothole) and a fall without an impact (a standstill drop) are *not* crashes; the
    second one is still reported as TIPPED.
- **Corrupt reads:** the driver rejects any burst with 3 or more words equal to `0xFF00`.
  The monitor also drops the older all-equal-gyro signature. Every threshold needs either
  2 consecutive samples or a hold time.
- **IMU setup:** ±8 g (so impacts don't clip at 4 g), ±500 °/s, 44 Hz DLPF, 100 Hz. Hardware
  wake-on-motion is set at **60 mg** (about 6× the noise), latched on INT, which is
  active-high push-pull into GPIO 33. Deep-sleep wake on that pin lands together with the
  supervisor's deep sleep.
- **Tunables** (ADR-0013, additive keys): `moveDeg` 1–20°, `tipDeg` 30–85°, `crashG`
  1.5–7.5 g, `crashCancel` 10–120 s. The hold and settle times are fixed for now.

## Consequences
- The security layer exists end to end on the bench: calibrate, events, crash pending,
  cancel and confirm. The actual alert delivery (the SMS via the tablet, the parked BLE relay
  queue) hooks into `MEV_CRASH_CONFIRMED`, `MEV_MOVED` and `MEV_TIPPED` later.
- "Was moving" is approximated by "the engine ran within 10 s". This is wrong for a bike
  idling at a standstill that gets knocked over: the result is TIPPED, plus a crash only if
  there was also a 3 g impact. A real speed signal (SDS or GPS) replaces the approximation
  later.
- The thresholds come from bench noise, not from the bike. Engine vibration, the side stand
  and a real drop must be measured on the bike before trusting MOVED, BUMP and crashG.
  Calibration must be redone once the board is mounted.
- The ~1 % corrupt-read rate is filtered but not explained. It is likely the long
  breadboard wiring to two modules; re-check it on the PCB.

## Alternatives considered
- **The MPU's DMP / sensor-fusion firmware:** heavier, undocumented blobs, and gravity-only
  tilt is enough for parked and lying-down decisions.
- **Min–max stillness check for calibration:** rejected after the bench showed one bad
  sample failing a still calibration.
- **A crash on impact alone:** potholes and kerbs would send SMSs. **A crash on tilt
  alone:** standstill drops would send SMSs. Requiring both, plus a 30 s cancel window,
  matches the roadmap's corroboration rule.
- **±4 g range:** keeps more resolution, but crash impacts clip. ±8 g still resolves about
  0.25 mg, far below the 3° moved threshold.
