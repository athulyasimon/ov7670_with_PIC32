## Interfacing the OV7670 with the PIC32

The goal of this project was interface the OV7670 camera with the PIC32MX595F512L microncontroller (housed in the NU32 Development Board). A full write up on the theory can be found on my [portfolio page](http://athulyasimon.github.io/project_portfolio/projects/a_camera_pic/)

There are a few lines to comment/uncomment in the display image function based on if you are seeing the output through Matlab or using Terminal's Screen. If using terminal I recommend decreasing the number of outputs so that you can actually make sense of the numbers seen. For the output, the first number is the number of rows in the image. This is correctly being diplayed as 145 rows when using the QCIF resolution. The remaining numbers in the first column show the number of columns in each row. This number is greatly affected by the clock speed and any prescalers that may be applied. For the QCIF resolution there should be 352 columns (176 x 2).

To run the Matlab code you must have Matlab open with root access. Otherwise Matlab will not be able to connect with the PIC32 through serial communication. 


The pin Connections between the PIC32 and the camera can be seen below

 | | 
 --------------|-------------
![Pin connections](https://raw.githubusercontent.com/athulyasimon/project_portfolio/gh-pages/public/images/ov7670_project/pin%20connections.jpg) | ![Wiring image](https://raw.githubusercontent.com/athulyasimon/project_portfolio/gh-pages/public/images/ov7670_project/wiring_diagram_resized.png)

