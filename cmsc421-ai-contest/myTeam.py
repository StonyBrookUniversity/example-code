# myTeam.py
# ---------
# Licensing Information: Please do not distribute or publish solutions to this
# project. You are free to use and extend these projects for educational
# purposes. The Pacman AI projects were developed at UC Berkeley, primarily by
# John DeNero (denero@cs.berkeley.edu) and Dan Klein (klein@cs.berkeley.edu).
# For more info, see http://inst.eecs.berkeley.edu/~cs188/sp09/pacman.html
#
# Author: Eric Norris <erictnorris@gmail.com>

from captureAgents import CaptureAgent
from game import Directions
from game import Actions
from util import nearestPoint
import random, time, util
import game
import math

#################
# Team creation #
#################

def createTeam(firstIndex, secondIndex, isRed,
               first = 'DummyAgent', second = 'DummyAgent'):
  """
  This function should return a list of two agents that will form the
  team, initialized using firstIndex and secondIndex as their agent
  index numbers.  isRed is True if the red team is being created, and
  will be False if the blue team is being created.

  As a potentially helpful development aid, this function can take
  additional string-valued keyword arguments ("first" and "second" are
  such arguments in the case of this function), which will come from
  the --redOpts and --blueOpts command-line arguments to capture.py.
  For the nightly contest, however, your team will be created without
  any extra arguments, so you should make sure that the default
  behavior is what you want for the nightly contest.
  """
  locator = AgentLocator()
  targeter = TargetCoordinator()
  agentOne = TargetAgent(firstIndex)
  agentTwo = TargetAgent(secondIndex)
  agentOne.initializeLocator(locator)
  agentTwo.initializeLocator(locator)
  agentOne.initializeTargeter(targeter)
  agentTwo.initializeTargeter(targeter)
  return [agentOne, agentTwo]

###########
# Locator #
###########
class AgentLocator():
  def __init__(self):
    self.team = []
    self.opponents = []
    self.states = []
    self.beliefs = []
    self.red = False
    self.distancer = None
  
  def initializeState(self, gameState, distancer, index):
    if len(self.states) != 0:
      return
    
    self.states.append(gameState)
    self.red = gameState.isOnRedTeam(index)
    self.distancer = distancer
    
    for index in range(gameState.getNumAgents()):
      self.beliefs.append(None)
      if gameState.isOnRedTeam(index) == self.red:
        self.team.append(index)
      else:
        self.opponents.append(index)
    
    self.legalPositions = [p for p in gameState.getWalls().asList(False)]
    self.__initializeBeliefs()
  
  def __initializeBeliefs(self):
    for index in self.opponents:
      self.beliefs[index] = util.Counter()
      position = self.states[-1].getInitialAgentPosition(index)
      self.beliefs[index][position] = 1.0
  
  def initializeUniformly(self, index):
    newBelief = util.Counter()
    for position in self.legalPositions:
      newBelief[position] = 1
    newBelief.normalize()
    self.beliefs[index] = newBelief
  
  def getAgentPosition(self, agentIndex):
    return self.beliefs[agentIndex].argMax()
  
  # defined as the closest agent to becoming a pacman, or an agent
  # who is already a pacman. Could use improving.
  def isOnOffensive(self, agentIndex):
    if self.states[-1].getAgentState(agentIndex).isPacman:
      return True
    
    agentDistance = util.Counter()
    defendingPos = self.states[-1].getInitialAgentPosition(self.team[0])
    for opponent in self.opponents:
      agentDistance[opponent] = -self.distancer.getDistance(self.getAgentPosition(opponent), defendingPos)
    
    return agentIndex == agentDistance.argMax()
  
  def getPositionDistribution(self, agentIndex, position):
    distribution = util.Counter()
    neighbors = Actions.getLegalNeighbors(position, self.states[-1].getWalls())
    if self.isOnOffensive(agentIndex):
      # Use offensive probability weights
      foodDistance = []
      foodList = self.states[-1].getRedFood().asList() if self.red else self.states[-1].getBlueFood().asList()
      maxWeight = math.pow(len(neighbors), 5)
      
      for neighborPos in neighbors:
        # Chances are the other agent will not come to a stop.
        if neighborPos == position:
          foodDistance.append((999, neighborPos))
          continue
        minDistance = min([self.distancer.getDistance(neighborPos, food) for food in foodList])
        foodDistance.append((minDistance, neighborPos))
      foodDistance.sort()
      
      for distance, neighbor in foodDistance:
        distribution[neighbor] = maxWeight
        maxWeight /= 5
    else:
      # Use defensive probability weights
      agentDistance = []
      positionList = [self.states[-1].getAgentPosition(index) for index in self.team]
      maxWeight = math.pow(len(neighbors), 5)
      
      for neighborPos in neighbors:
        # Chances are the other agent will not come to a stop.
        if neighborPos == position:
          agentDistance.append((999, neighborPos))
          continue
        minDistance = min([self.distancer.getDistance(neighborPos, agentPosition) for agentPosition in positionList])
        agentDistance.append((minDistance, neighborPos))
      agentDistance.sort()
      
      for distance, neighbor in agentDistance:
        distribution[neighbor] = maxWeight
        maxWeight /= 5
    distribution.normalize()
    return distribution
  
  # Returns the agent acting before the specified agent
  def previousAgent(self, index):
    return (index - 1) % self.states[-1].getNumAgents()
  
  def elapseTime(self, agentIndex):
    newBelief = util.Counter()
    for position in [nonZeroPos for nonZeroPos in self.legalPositions if self.beliefs[agentIndex][nonZeroPos] != 0]:
      positionDistribution = self.getPositionDistribution(agentIndex, position)
      for newPosition in positionDistribution:
        newBelief[newPosition] += positionDistribution[newPosition] * self.beliefs[agentIndex][position]
    self.beliefs[agentIndex] = newBelief
  
  def observe(self, gameState, observingAgent):
    if len(self.states) == 1:
      # This is the first observe function called, so do not elapseTime
      self.states.append(gameState)
    else:
      # This is NOT the first observe function called, so elapseTime for previous agent
      self.elapseTime(self.previousAgent(observingAgent))
      self.states.append(gameState)
    
    for opponent in self.opponents:
      agentPosition = gameState.getAgentPosition(opponent)
      if agentPosition:
        correctBelief = util.Counter()
        correctBelief[agentPosition] = 1.0
        self.beliefs[opponent] = correctBelief
        continue
      
      noisyDistance = gameState.getAgentDistances()[opponent]
      pacmanPosition = gameState.getAgentPosition(observingAgent)
      newBelief = util.Counter()
      
      for position in [nonZeroPos for nonZeroPos in self.beliefs[opponent].keys() if self.beliefs[opponent][nonZeroPos] != 0]:
        trueDistance = abs(position[0] - pacmanPosition[0]) + abs(position[1] - pacmanPosition[1])
        newBelief[position] = gameState.getDistanceProb(trueDistance, noisyDistance) * self.beliefs[opponent][position]
      
      if gameState.getAgentState(opponent).isPacman:
        # We know for a fact the agent is on my half of the grid
        for position in newBelief.keys():
          if self.red != gameState.isRed(position):
            newBelief[position] = 0
      else:
        # We know for a fact the agent is on their half of the grid
        for position in newBelief.keys():
          if self.red == gameState.isRed(position):
            newBelief[position] = 0
      
      if sum(newBelief.values()) == 0:
        self.initializeUniformly(opponent)
      else:
        newBelief.normalize()
        self.beliefs[opponent] = newBelief

########################
# Target Communication #
########################
class TargetCoordinator():
  def __init__(self):
    self.targets = dict()
  
  def setTarget(self, agent, position):
    self.targets[agent] = position
  
  def getTarget(self, agent):
    return self.targets[agent]
  
  def isDuplicateTarget(self, index, position):
    for agent in [otherAgent for otherAgent in self.targets.keys() if otherAgent != index]:
      if self.getTarget(agent) == position:
        return True
    else:
      return False

##########
# Agents #
##########
class TargetAgent(CaptureAgent):
  def registerInitialState(self, gameState):
    CaptureAgent.registerInitialState(self, gameState)
    self.locator.initializeState(gameState, self.distancer, self.index)
  
  def initializeLocator(self, locator):
    self.locator = locator
  
  def initializeTargeter(self, targeter):
    self.targeter = targeter
  
  def getSuccessor(self, gameState, action):
    # Finds the next successor which is a grid position (location tuple).
    successor = gameState.generateSuccessor(self.index, action)
    pos = successor.getAgentState(self.index).getPosition()
    if pos != nearestPoint(pos):
      # Only half a grid position was covered
      return successor.generateSuccessor(self.index, action)
    else:
      return successor
  
  def chooseAction(self, gameState):
    self.locator.observe(gameState, self.index)
    self.chooseTarget(gameState)
    return self.bestTargetAction(gameState)
    
  def bestTargetAction(self, gameState):
    targetPos = self.targeter.getTarget(self.index)
    # Choose the action that gets us closer to the target
    actionResults = util.Counter()
    actions = gameState.getLegalActions(self.index)
    actions.remove(Directions.STOP)
    for action in actions:
      successor = self.getSuccessor(gameState, action)
      newPos = successor.getAgentPosition(self.index)
      actionResults[action] = -self.getMazeDistance(newPos, targetPos)
    return actionResults.argMax()
  
  def chooseTarget(self, gameState):
    myState = gameState.getAgentState(self.index)
    myPos = gameState.getAgentPosition(self.index)
    self.invaders = [i for i in self.getOpponents(gameState) if gameState.getAgentState(i).isPacman]
    self.ghosts = [g for g in self.getOpponents(gameState) if not gameState.getAgentState(g).isPacman]
    
    # Check to see if I am a ghost
    if not myState.isPacman and len(self.invaders) != 0:
      # Make sure I am not scared
      if myState.scaredTimer == 0:
        # Hunt down the nearest invader
        index, invaderTarget = self.nearestInvader(gameState)
        # If ghost is already targeted, pick random invader
        if self.targeter.isDuplicateTarget(self.index, invaderTarget):
          if len(self.invaders) > 1:
            tryCount = 0
            while self.targeter.isDuplicateTarget(self.index, invaderTarget) and tryCount < 3:
              invaderTarget = self.locator.getAgentPosition(random.choice(self.invaders))
              tryCount += 1
            self.targeter.setTarget(self.index, invaderTarget)
            return
        else:
          self.targeter.setTarget(self.index, invaderTarget)
          return
        # If no other invaders, act like a pacman (drop through if statement)
    
    nearbyEnemy = self.nearbyEnemy(gameState)
    if nearbyEnemy:
      index, enemyPos, distance = nearbyEnemy
      # If there is a power pellet closer than the enemy, go for it
      nearestCapsule = self.nearestCapsulePos(gameState)
      if nearestCapsule:
        distance = self.getMazeDistance(myPos, nearestCapsule)
        if distance <= 3:
          self.targeter.setTarget(self.index, nearestCapsule)
          return
      # If the ghost is scared, target it
      if gameState.getAgentState(index).scaredTimer > 1:
        # But only if we are not terribly far from food
        nearestFood = self.nearestFoodPos(gameState)
        if self.getMazeDistance(myPos, nearestFood) > 6:
          self.targeter.setTarget(self.index, nearestFood)
          return
        ghostTarget = enemyPos
        if not self.targeter.isDuplicateTarget(self.index, ghostTarget):
          self.targeter.setTarget(self.index, enemyPos)
          return
      # Otherwise, take all of the closest food, and pick the one farthest
      # from the enemy.
      foodList = self.getFood(gameState).asList()
      foodDistance = [self.getMazeDistance(myPos, food) for food in foodList]
      minDistance = min(foodDistance)
      closestFood = [f for f, d in zip(foodList, foodDistance) if d == minDistance]
      
      # We now have a list of the closest food
      enemyFoodDistance = util.Counter()
      for food in closestFood:
        enemyFoodDistance[food] = self.getMazeDistance(enemyPos, food)
      self.targeter.setTarget(self.index, enemyFoodDistance.argMax())
    else:
      foodTarget = self.nearestFoodPos(gameState)
      if self.targeter.isDuplicateTarget(self.index, foodTarget):
        # Pick the food farthest from this food
        foodDistance = util.Counter()
        for food in self.getFood(gameState).asList():
          foodDistance[food] = self.getMazeDistance(foodTarget, food)
        self.targeter.setTarget(self.index, foodDistance.argMax())
      else:
        self.targeter.setTarget(self.index, foodTarget)
  
  def nearestInvader(self, gameState):
    if len(self.invaders) == 0:
      return None
    
    invaderDistance = util.Counter()
    myPos = gameState.getAgentPosition(self.index)
    for invader in self.invaders:
      position = self.locator.getAgentPosition(invader)
      invaderDistance[invader] = -self.getMazeDistance(position, myPos)
    nearestInvader = invaderDistance.argMax()
    return (nearestInvader, self.locator.getAgentPosition(nearestInvader))
  
  def nearestGhost(self, gameState):
    if len(self.ghosts) == 0:
      return None
    
    ghostDistance = util.Counter()
    myPos = gameState.getAgentPosition(self.index)
    for ghost in self.ghosts:
      position = self.locator.getAgentPosition(ghost)
      ghostDistance[ghost] = -self.getMazeDistance(position, myPos)
    nearestGhost = ghostDistance.argMax()
    return (nearestGhost, self.locator.getAgentPosition(nearestGhost))
  
  def nearbyEnemy(self, gameState):
    # Nearby is defined as within 6 maze distance
    NEARBY_THRESHOLD = 6
    myState = gameState.getAgentState(self.index)
    myPos = gameState.getAgentPosition(self.index)
    if myState.isPacman and len(self.ghosts) != 0:
      # Nearby enemy is a ghost
      ghost = self.nearestGhost(gameState)
      if ghost:
        index, enemyPos = ghost
        # Calculate the distance to the ghost
        distance = self.getMazeDistance(myPos, enemyPos)
        if distance <= NEARBY_THRESHOLD:
          return (index, enemyPos, distance)
    elif myState.scaredTimer > 0 and len(self.invaders) != 0:
      # Nearby enemy is a powered-up pacman
      invader = self.nearestInvader(gameState)
      if invader:
        index, invaderPos = invader
        # Calculate the distance to the pacman
        distance = self.getMazeDistance(myPos, invaderPos)
        if distance <= NEARBY_THRESHOLD:
          return (index, invaderPos, distance)
    return None

  def nearestCapsulePos(self, gameState):
    capsules = self.getCapsules(gameState)
    if len(capsules) == 0:
      return None
    
    capsuleDistance = util.Counter()
    myPos = gameState.getAgentPosition(self.index)
    for capsule in capsules:
      capsuleDistance[capsule] = -self.getMazeDistance(myPos, capsule)
    return capsuleDistance.argMax()

  def nearestFoodPos(self, gameState):
    foodList = self.getFood(gameState).asList()
    if len(foodList) == 0:
      return None
    
    foodDistance = util.Counter()
    myPos = gameState.getAgentPosition(self.index)
    for food in foodList:
      foodDistance[food] = -self.getMazeDistance(myPos, food)
    return foodDistance.argMax()
