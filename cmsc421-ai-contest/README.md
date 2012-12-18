cmsc421-ai-contest
============

<em>CMSC421 Project 5: Pacman Capture the Flag (http://www.cs.umd.edu/class/fall2011/cmsc421/projects/contest/contest.html)</em>

Description / Overview
---
CMSC421 (Introduction to Artificial Intelligence) has a series of projects that involve writing various artificial intelligence systems. Written in Python (2.7), each project was derived from the Pac-man Projects (http://www-inst.eecs.berkeley.edu/~cs188/pacman/pacman.html) from UC Berkeley. Project 5 was the final project: a contest between students to see who could come up with the best artificial intelligence agent in a custom Pac-man game.

In this game each team has one half of the board with food pellets that the other team will try to eat. Each team is made up of two agents. When an agent is on "home turf", it is a Ghost, and can eat the enemy Pac-man. The reverse is also true; when an agent is on "enemy territory", it is a Pac-man, and can be eaten by the enemy Ghosts. The goal is to eat more than the other team. There are various other nuances to the game, like the fact that your agent is not given the position of the other agents - only the noisy distance to them.

The AI agent I came up with (titled PrincessCelestia) took first place in the final rankings (http://www.cs.umd.edu/class/fall2011/cmsc421/projects/contest/results/final.html). 

As mentioned in the GitHub repo README, the interesting code is in /myTeam.py.

Thoughts / Experience
---
I had a great time working on this project and spent many a night staying up into the early hours coding and testing my Pac-man agents. The fact this was a contest only added to my determination of creating the best agent possible. To do so I first came up with a list of fundamental problems:
* Determining locations of the competing agents
* Eating all of the food on the competing side
* Defending the food on my own side
* Avoiding being eaten by the competing agents

### Determining Locations of Competing Agents
Since an agent is only given a noisy distance to competing agents, I decided to use Bayesian inference create a probability distribution of where they could be located. What I consider my most clever move, however, was to share this information between my two agents - since both would be updating the probabilities of locations, it would paint a more accurate picture of the playing field.

### Difficulties
Early on in the Project I ran into some difficulties writing code that was not too ambitious - it was hard to find a good tradeoff of simplicity and utility. Often if I wrote too complex of an agent, the code would be far too slow to run in a contest setting. Otherwise I would write code that was too simple and could not perform well enough to beat competing agents.

What I found that worked was to simplify the problem itself, and then write code to best attack it. For the purpose of this project, I decided the simplest form was that my agents needed to decide where to move - they had to have a "target" of sorts. Additionally, the agents should share their target with eachother to make sure they are not targeting the same thing.

### Conclusion
The combination of accurate locations and smart but simple decision making is what I feel caused my agent to outperform the others. To improve the agent I might add some reinforcement learning, in order to better "train" it to play against other competing players.
