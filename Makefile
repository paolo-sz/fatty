exe:
	cd src; $(MAKE)

clean:
	cd src; $(MAKE) clean

checksrc:
	cd src; $(MAKE) checksrc

.NOTPARALLEL:  
