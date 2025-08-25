## Objective.

To collect real-time data from ESP32 IoT devices, send it to our server, and push that data into Microsoft Dynamics 365 for use in dashboards, alerts, and analytics.

### Install the application after first clone.

Run the following.
```bash
git clone https://github.com/ahosny333/APC_IoT.git
cd APC_IoT
git submodule update --init --recursive
```
```bash
cd esp-idf\
install.bat 
cd ..
cd components\arduino\tools
python get.py
```
### Compile and flash the project.

Run the following.
```bash
cd esp-idf\
export.bat
cd ..                             # Go to the project directory
idf.py build                      # Compile the Project  
idf.py -p PORT flash              # Flash the Project
idf.py -p PORT monitor            # Monitor the Project
idf.py -p PORT flash monitor      # Flash & monitor the Project
```

### Troubleshooting

* Program upload failure
* Hardware connection is not correct: run `idf.py -p PORT monitor`, and reboot your board to see if there are any output logs.
* The baud rate for downloading is too high: lower your baud rate in the `menuconfig` menu, and try again.
