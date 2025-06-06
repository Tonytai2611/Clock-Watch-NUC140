# ⏱ Clock-Watch for NUC140

A fully-featured **digital stopwatch system** built on the NUC140 development board. This project replicates a traditional stopwatch and adds features like **alarm setting**, **lap recording**, and visual/audible feedback using **LEDs, 7-segment displays, and a buzzer**.

---

## 🚀 Features

### 🟢 1. Idle Mode
- Default mode after reset.
- **LED 5** is ON to indicate Idle Mode.
- Awaits user input to enter other modes.

### ⏰ 2. Alarm Set Mode
- Enter by pressing **Key K3**.
- **LED 8** lights up (others OFF).
- Press **PB15** to increase alarm time (0–59 seconds).
- Alarm is displayed on the **4-digit 7-segment display**.
- Press **K3** again to save and return to Idle Mode.

### 🕒 3. Count Mode (Stopwatch)
- Activate with **Key K1**.
- **LED 6** turns ON.
- Stopwatch starts with **0.1-second precision** via **Timer0 interrupt**.
- Display shows elapsed time.
- Buzzer rings **3 times** when time matches alarm.
- **K1** toggles pause/resume.
- In Pause Mode: **LED 7** lights up.

### 📍 4. Lap Time Recording
- Press **Key K9** during Count Mode to **save lap times**.
- Max **5 lap records** (oldest overwritten).
- View laps in Check Mode.

### 🔍 5. Check Mode (View Laps)
- Enter by pressing **Key K5** in Pause Mode.
- Display format:
  - First digit: **Lap number**
  - Last three digits: **Lap time (x0.1s)**
- Press **PB15** to navigate between laps.
- Exit with **Key K5**.

### 🔄 6. Reset
- Press **Key K9** while paused to **reset** and return to Idle Mode.

---

## ⚙️ Implementation Details

### 🔧 Hardware
- Based on **NUC140 microcontroller**.
- Uses GPIO for:
  - **Button input**
  - **LED control**
  - **7-segment display**
- **Timer0** + **external interrupts** handle timing and input.

### 🧠 Software Architecture
- **Finite State Machine (FSM)** controls all mode transitions.
- **Timer0 ISR** updates time at 0.1s intervals.
- **Modularized design** for:
  - GPIO setup
  - Display output
  - Mode logic
  - Alarm/buzzer management

---

## 🛠 Usage Instructions

### 1. Clone the Repository
```bash
git clone https://github.com/your-username/Clock-Watch-NUC140.git
