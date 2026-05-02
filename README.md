CYTos: Cheap Yellow Terminal Operating System

### this is the oficial repository for the CYTos (mainely codet with AI)

It contains many features, such as:
- CHIP-8 Emulator
- Wi-Fi-based Chat (which I will keep functional between versions)
- I2C Scanner
- Text Editor
- Drawing App
- ...and other features! 

Just look at the loop in `terminal.ino` or type `help` in the terminal and look at the files being generated. Sadly, the code is so large that it uses almost all of the available flash memory. In later versions, I will probably include an interpreter for a BASIC-like language for extra apps.

---

### Hardware Requirements
1. CYD (Cheap Yellow Display): If you need information about this, check out [this repository](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display).

### Optional Hardware
1. I2C Sensors: You can add sensors that support I2C. 
2. Audio: You need a buzzer or a piezo element, which you can connect to the sound output port of the CYD.
3. Power: A Li-ion battery with charging electronics and a switch for turning it on and off.

---

### Ideas to Improve This Project

If you want to contribute or improve the system, here are some ideas:

#### Easy:
1. Add an interpreter for a BASIC-like programming language.
2. Add support for LoRa or similar technologies.
3. Add custom themes.
4. Improve the drawing app.
5. Add a screenshot feature.
6. Add a shutdown command to end every background process on the other CPU core.

#### Hard:
1. Rewrite the code to be better (faster, using custom libraries, etc.).
2. Rewrite the core in Assembly so it is much faster.
3. Add a text-based browser.
4. bash interpretter

---

### Contribution Guidelines
Feel free to do whatever you want with the code. If possible, try to keep everything compatible with older releases—or don't, I don't mind! It is just a request.


![Reference](/image0.jpg)
![Reference](/image1.jpg)
![Reference](/image2.jpg)
![Reference](/image3.jpg)
![Reference](/image4.jpg)
