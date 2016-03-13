MMLIB = mmlib
INCLUDE = include
ROUTER = router
ROUTER_PROXY = router_proxy
APP = app
LOGSVR = logsvr

MAKE = make
CLEAN = clean
INSTALL = install

all:
	cd $(MMLIB) && $(MAKE)
	cd $(INCLUDE) && $(MAKE)
	cd $(ROUTER) && $(MAKE)
	cd $(ROUTER_PROXY) && $(MAKE)
	cd $(APP) && $(MAKE)
	cd $(LOGSVR) && $(MAKE)

clean:
	cd $(MMLIB) && $(MAKE) $(CLEAN)
	cd $(INCLUDE) && $(MAKE) $(CLEAN)
	cd $(ROUTER) && $(MAKE) $(CLEAN)
	cd $(ROUTER_PROXY) && $(MAKE) $(CLEAN)
	cd $(APP) && $(MAKE) $(CLEAN)
	cd $(LOGSVR) && $(MAKE) $(CLEAN)

