# ESP32-CAM → ThingsBoard Snapshot Demo
A complete open-source demo showing how to send JPEG snapshots from an ESP32-CAM to ThingsBoard using MQTT.  
Images are processed by AI (Edge Impulse Cloud API), and results appear in the ThingsBoard dashboard.

---

## 🚀 Features
- 📷 ESP32-CAM captures QQVGA JPEG images
- 🔁 Sends snapshot every 3 seconds via MQTT
- ☁️ ThingsBoard receives & displays images
- 🧠 AI Cloud inference (via Edge Impulse REST API)
- 📊 Dashboard to visualize images + AI result
- 🛠 Expandable for robot control (STM32 or ESP32)

---

## 🏗 Architecture
ESP32-CAM → ThingsBoard MQTT → Rule Engine → Edge Impulse AI → Dashboard

---

## 📁 Repository Structure
firmware/ # ESP32-CAM Arduino code
thingsboard/ # Dashboard + Rule Engine JSON
docs/ # Architecture, images, guides
demo/ # Example results

---

## 🔧 Hardware Requirements
- ESP32-CAM AI Thinker
- USB-TTL programmer
- WiFi 2.4GHz
- ThingsBoard Cloud account
- Edge Impulse project (optional AI)

---

## 🔌 Setup Instructions

### 1. Flash ESP32-CAM
firmware/esp32cam_tb_snapshot.ino
### 2. Import ThingsBoard Dashboard  
Import:
thingsboard/dashboard.json
thingsboard/rule_chain.json

### 3. Configure Edge Impulse API  
Edit Rule Engine → Rest API Call with your API key.

---

## 🧠 AI Cloud Processing
ThingsBoard Rule Engine forwards JPEG →  
Edge Impulse REST API: `/api/testing/classify`  
AI returns probabilities → Dashboard displays result.

---

## 🖼 Demo Images
See `/demo` folder.

---

## 📜 License
MIT License.
