from mininet.topo import Topo

class Tree(Topo):
	def __init__(self, n=3, m=4):
		Topo.__init__(self)

		core = self.addSwitch('core1')
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



topos={'tree':(lambda n, m:Tree(n, m))}
