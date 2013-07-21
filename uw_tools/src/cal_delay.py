#!/usr/bin/python
# -*- coding: utf-8 -*-
from math import sqrt
SOUND_SPEED= 1500.0
width = 50
tranDistance = 100.0
fwd_list = [(290,-43,0),(240,49,0),(290,-43,0),(240,49,0),(290,-43,0),(330,49,0),(330,49,0),(380,-40,0)]
nod_list = [(200,0,0),(200,0,0),(330,49,0),(380,-20,0),(380,-20,0),(380,-40,0),(415,0,0),(415,0,0)]

class Node(object):
  
    def __init__(self,x_,y_,z_,fx_,fy_,fz_):
        self.x = x_
        self.y = y_
        self.z = z_
        self.fx = fx_
        self.fy = fy_
        self.fz = fz_
        self.sx = 130.0
        self.sy = 0.0
        self.sz = 0.0
        self.tx = 415.0
        self.ty = 0.0
        self.tz = 0.0

    def display(self):
        #print "Advance:",self.advance()
        #print "Project:",self.projection()
        #print "Distance:",self.distance()
        print "Delay:",self.calDelay()
        #print "d2:",(tranDistance - self.distance())/SOUND_SPEED
        #print "distance:",self.distance(),"Real Timeout:",self.calTimeout()
        
    def distance(self):
        return sqrt((self.fx-self.x)*(self.fx-self.x) + (self.fy-self.y)*(self.fy-self.y) + (self.fz-self.z)*(self.fz-self.z))
        
    def advance(self):
        ad = sqrt((self.tx-self.x)*(self.tx-self.x) + (self.ty-self.y)*(self.ty-self.y) + (self.tz-self.z)*(self.tz-self.z))
        return ad
    
    def projection(self):
        wx = self.tx - self.sx
        wy = self.ty - self.sy
        wz = self.tz - self.sz
        vx = self.x - self.sx
        vy = self.y - self.sy
        vz = self.z - self.sz
        cpx = vy*wz - vz*wy
        cpy = vz*wx - vx*wz
        cpz = vx*wy - vy*wx
        area = sqrt(cpx*cpx + cpy*cpy + cpz*cpz)
        len = sqrt((self.tx-self.sx)*(self.tx-self.sx) + (self.ty-self.sy)*(self.ty-self.sy) + (self.tz-self.sz)*(self.tz-self.sz))
        return area/len
    
    def calDelay(self):
        dx = self.x - self.fx
        dy = self.y - self.fy
        dz = self.z - self.fz
        dtx = self.tx - self.fx
        dty = self.ty - self.fy
        dtz = self.tz - self.fz
        dp = dx*dtx + dy*dty + dz*dtz
        p = self.projection()
        d = sqrt(dx*dx + dy*dy + dz*dz)
        l = sqrt(dtx*dtx + dty*dty + dtz*dtz)
        cos_theta = dp/(d*l)
        delay = (p/width) + (tranDistance - d*cos_theta)/tranDistance
        return delay

    def calTimeout(self):
        soundTranDelay = (tranDistance - self.distance())/SOUND_SPEED
        delay = self.calDelay()
        if delay > 0:
            return sqrt(delay) + soundTranDelay*2;

for i in range(0,7):
    node = Node(nod_list[i][0],nod_list[i][1],nod_list[i][2],fwd_list[i][0],fwd_list[i][1],fwd_list[i][2])
    node.display()
