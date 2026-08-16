# 7/2/2026 - Finished the prototype

_Time spent: 4.0h_  

I woke up to a door slamming and I really didn't have a clue what it was. But it turned out to be GLS with my order!  
So with my brother we imedeatly opened it up and was really exited that the sensor came in.  
After breakfest I imeadedly went to my worksop and connected the sensor, also I have triple checked everything BC this sensor can't get fried.  
So after hooking it up I've uploaded the test code from SEN6x library and it worked first try!  
And after that succes I've coded I've imedeatly started coding code for it. I mean I just copied code blocks from the example and hooked everything to the coresponding values. And at first the aligment was wonky but with some tweaks later it looks pretty good.  

![First prototype done](https://cdn.hackclub.com/019f23f8-23f4-7395-831d-e00cda036d53/img_20260702_195404.jpg)

# 7/2/2026 - Made an all in one case

_Time spent: 2.0h_  

Last time I did a minimalistic case for it, but now I wanted to have everything contained. So I designed a super goofy case for it! YAY But it houses everything ig.  
I'm not that proud but should get the job done. it looks like a freaking frog.  
Also FreeCAD anoys me bc it doesn't support text. But Prusaslices came for a rescue. 	

![CAse](/Images/AllinOne-Front.png)  
![CAse](/Images/AllinOne-Back.png)  
![PrusaSlicer](https://cdn.hackclub.com/019f2465-c498-7049-bd8e-aa67cfb0bf20/image.png)  

# 7/11/2026 - Integration to the case and a bit of code

_Time spent: 6.7h_

![](https://cdn.hackclub.com/019f56b4-3267-7160-8f68-5d4489402976/img_20260703_142842.jpg)  
![](https://cdn.hackclub.com/019f56b4-30cc-711f-a745-fc04d2e079a7/img_20260703_160435.jpg)  

Allright so after an amazing family vacation...  

![](https://cdn.hackclub.com/019f56ba-bd40-7b2c-9203-1a922db5f932/1000038880.jpg)

I've got home and I found the package from lakakit home thanks to my dad bc he grabbed from the post office. Also I have printed the case before the vacation and cleaned up the suports.   
I was super happy about building it!!!! Like let's goooo and let's build it.  
![](https://cdn.hackclub.com/019f562b-3fe5-7383-bac7-0e8b8a8a8873/laskakit_package.jpg)  
So I've started working on it ASAP, bc I'm super exited about it.  
Started with a rough assembly to find problems and there were a lot of problems lol bc I was rushing it.  

**Problems found:**
- Hole for the GPS it soo tall
- Hole for the GPS cable is too small
- GPS cable toooooo short
- Didn't tolerance the SEN66 sensor in any shape or form.
- Battery is too long wth
- Looks like a frog
- No place for the antenna - TSA might look at me wrong fah
- No passthrough for battery cable

Oh man IDK I'm rushing it madmax style.  
So I've made the cable longer (Which was a pain in the hack) managed to get it passed through by scraping a bit off the connector. FAHHHH  
![](https://cdn.hackclub.com/019f562b-3d83-77fa-aeb7-02d7d0314301/gps_cable.jpg)  
![](https://cdn.hackclub.com/019f562b-42d2-77a0-b0d6-a99054abc6b9/gps_cable_extension_2.jpg)  
![](https://cdn.hackclub.com/019f562b-4889-71c6-a4b4-94958df9a54f/fah.jpg)  

Oh and then there was ZEN as the SEN66 assebly was a lot more smooooother.  
It fitted so smoothly and It worked almost first try niiiiceee.  
So I have tried the example sketch on it but it didn't work as I2C pins aren't prob defined in arduino same as in meshtastic.  So with a bit of help from AI I've managed to insert one line to define the pins and it worked out good.  

![](https://cdn.hackclub.com/019f562b-4a6e-751a-aee6-ff2d5b4f1a56/wiring.png)  
![](https://cdn.hackclub.com/019f562b-44c1-7d95-87ed-5a3ce539c900/t114_prototype.jpg.jpg)  

# 7/12/2026 - Made an all in one case

_Time spent: 4h_

Allrighty so after yeasterday succes I had today tried codind some code for it. Like I have a lot of thing to prepare for the hackathon so I'm going to vibecode it with claude. And yeah it worked wonderfully. The UI is clean and it works.  
I had one hurdle with ADC as AI thought it was the correct code for t114 but it was for the e290.  
I have also tried to add in GPS support but I ran out of contex and tokens.  

Looks super neat, and I can't wait to measure the nasty air at the venue lol.  
![](https://cdn.hackclub.com/019f56bc-9ca5-7faf-82b0-7bfb11f43d43/1000039036.jpg)  
[Video of it working](https://cdn.hackclub.com/019f56bc-70db-7756-8306-433a7e309648/1000039035.mp4)  
