all: psyq400 psyq470

psyq400: psyq400.tar.gz
	mkdir -p $@
	cd $@ && tar -xf ../$^
psyq400.tar.gz:
	wget https://github.com/psyqz/psyz/releases/download/requirements/psyq400.tar.gz
	sha256sum -c $@.sha256
	
psyq470: Psy-Q_47.zip
	mkdir -p $@
	cd $@ && unzip ../$^
	mv psyq470/LIB/Libcd.lib psyq470/LIB/LIBCD.LIB
Psy-Q_47.zip:
	wget https://github.com/psyqz/psyz/releases/download/requirements/$@
	sha256sum -c $@.sha256
