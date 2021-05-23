from mininet.topo import Topo

"""class Aggregation(Topo):
	
	def __init__(self, n):
		 
		 Topo.__init__(self)
		 
		 self.switch1 = self.addSwitch('s' + str(n))
		 self.switch2 = self.addSwitch('s' + str(n + 1))
		 self.switch3 = self.addSwitch('s' + str(n + 2))
		 self.switch4 = self.addSwitch('s' + str(n + 3))
		 
		 self.host1 = self.addHost('h' + str(n))
		 self.host2 = self.addHost('h' + str(n + 1))
		 self.host3 = self.addHost('h' + str(n + 2))
		 self.host4 = self.addHost('h' + str(n + 3))
		 
		 self.addLink(self.switch3, self.host1)
		 self.addLink(self.switch3, self.host2)
		 self.addLink(self.switch4, self.host3)
		 self.addLink(self.switch4, self.host4)
		 
		 self.addLink(self.switch1, self.switch3)
		 self.addLink(self.switch1, self.switch4)
		 self.addLink(self.switch2, self.switch3)
		 self.addLink(self.switch2, self.switch4)"""
		 

class DataCenter(Topo):
	
	def __init__(self, n, m):
		
		Topo.__init__(self)
		
		cores = []
		
		for i in range(n):
			core = self.addSwitch('s' + str(i + 1 + m * 4))
			cores.append(core)
		
		for i in range(m):
			k = 4 * i + 1
			switch1 = self.addSwitch('s' + str(k))
			switch2 = self.addSwitch('s' + str(k + 1))
			switch3 = self.addSwitch('s' + str(k + 2))
			switch4 = self.addSwitch('s' + str(k + 3))
			
			host1 = self.addHost('h' + str(k))
			host2 = self.addHost('h' + str(k + 1))
		 	host3 = self.addHost('h' + str(k + 2))
		 	host4 = self.addHost('h' + str(k + 3))
		 
		 	self.addLink(switch3, host1)
			self.addLink(switch3, host2)
		 	self.addLink(switch4, host3)
		 	self.addLink(switch4, host4)
		 
		 	self.addLink(switch1, switch3)
		 	self.addLink(switch1, switch4)
		 	self.addLink(switch2, switch3)
		 	self.addLink(switch2, switch4)
		 	
			for j in range(n):
				if j % 2 == 0:
					self.addLink(cores[j], switch1)
				else:
					self.addLink(cores[j], switch2)
		

class Tree(Topo):
	
	def __init__(self, n, m):
		print(n, m)

		Topo.__init__(self)
		
		core = self.addSwitch('s0')
		
		switch1 = self.addSwitch('s1')
		switch2 = self.addSwitch('s2')
		switch3 = self.addSwitch('s3')

		host1 = self.addHost('h1')
		host2 = self.addHost('h2')
		host3 = self.addHost('h3')
		host4 = self.addHost('h4')
		host5 = self.addHost('h5')
		
		self.addLink(core, switch1)
		self.addLink(core, switch2)
		self.addLink(core, switch3)

		self.addLink(switch1, host1)
		self.addLink(switch1, host2)
		self.addLink(switch2, host3)
		self.addLink(switch2, host4)
		self.addLink(switch3, host5)

TOPOS = {'treetopo':(lambda n,m:Tree(n, m)), 'datacentertopo':(lambda n, m:DataCenter(n, m))}
