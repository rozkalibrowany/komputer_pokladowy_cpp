# VOCC - desktop client for automotive (desktop, ARM)
![alt text](https://preview.ibb.co/eZY4c9/Zrzut_ekranu_z_2018_09_11_20_43_48.png)
![alt text](https://i.ibb.co/KFwDccx/2.png)
![alt text](https://i.ibb.co/QFyp5TT/Zrzut-ekranu-z-2018-12-26-13-02-07.png)
![alt text](https://i.ibb.co/FYqY7Qc/4.png)
# Requirements
 - gcc 
 - > Qt 5.9 (+QStyle, QtCharts)
 - qmake

# Compilation
* cd dev/
* qmake komp_pokl_cpp.pro
* make 

# Help
./komp_pokl_cpp -h

# Logger
	 -l <arg> - debug level 
		 LOG_MAIN 		 0x0001
		 LOG_GUI 		 0x0002
		 LOG_CONNECTIONS 	 0x0004
		 LOG_RPM 		 0x0008
		 LOG_MAINWINDOW 	 0x0010
		 LOG_ALERTS 		 0x0020
		 LOG_LEDINDICATOR 	 0x0040
		 LOG_SETTINGS 		 0x0080
		 LOG_CONNECTIONS_DATA 	 0x0100
		 LOG_MAINWINDOW_DATA 	 0x0200


# Output files
bin/komp_pokl_cpp

