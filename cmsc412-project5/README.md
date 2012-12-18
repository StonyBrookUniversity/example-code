cmsc412-project5
============

<em>CMSC412 Project 5: GOSFS - GeekOS File System (http://www.cs.umd.edu/class/fall2011/cmsc412/project5/index.html)</em>

Description / Overview
---
In CMSC412, I was tasked with a number of projects involving editing and adding functionality to GeekOS, an instructional operating system used at the University of Maryland. Project 5 involved adding a working file system to GeekOS - aptly named <strong>GOSFS</strong>, or GeekOS File System.

The basic structure of GOSFS had <em>file nodes</em>; similar to Unix's inodes, file nodes contained all of the information about a file or directory. A file in GOSFS had 10 blocks, where the first 8 were direct blocks, the 9th was an indirect block, and the 10th a double-indirect block. Directories used only the first block and contain a linear array of more file nodes.

As mentioned in the GitHub repo README, the code I wrote and implemented is in /src/geekos/gosfc.c and /include/geekos/gosfs.h.

Thoughts / Experience
---
Project 5 required the most work out of all of the projects in the class. My final implementation clocked in at 1037 sloc according to GitHub. One of my initial difficulties when working on this project was dealing with the scope of the code required - most of the functions I was expected to implement required other functions to be completed before they would be useful. Furthermore, it was challenging to write succinct and elegant code when dealing with reading and writing data to disk. In the end, I found the best solution to be twofold:

### Build a Heirarchy of Functionality: 
I started from the ground up, encapsulating code into functions that could be called and re-used in higher-order functions. For example, since I knew that GOSFS operated on 4KB "blocks" and the disk used 512-byte sectors, I wrote a function that read and wrote 8 disk blocks at a time. This allowed me to utilize more descriptive code - when I wanted to write out the superblock to disk, I would not need to use a for loop and iterate over the structure, instead I would simply call this utility function and pass in a pointer to the data. Additionally, failure points could be identified by drilling down to a specific function.

### Keep it Simple:
While performance is certainly an important aspect of any program, I learned that focusing too much on getting the best performance takes valuable time away from meeting a deadline. In the end, I decided to write my code as simply as possible and aim for a robust implementation as opposed to the absolute fastest implementation. The project was not going to be graded on performance, but instead on being able to pass a number of tests on the implementation of the file system. To borrow from Donald Knuth, "...premature optimization is the root of all evil." Focusing on writing clean code allowed me to score 100% and complete the project in time. Now that the base project is complete, I could easily go back and tune the code for better performance if necessary.

