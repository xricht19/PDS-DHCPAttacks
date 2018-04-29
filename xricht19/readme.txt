Autor: Jiří Richter
Datum: 26.4.2018
Předmět: Přenos dat, počítačové sítě a protokoly
Projekt: DHCP útoky
-------------------
Makefile
- programy přeloženy přiloženým Makefile souborem příkazem: make

DHCP Starvation
spouštění nutné s administrátorskými právy:
sudo ./pds-dhcpstarve -i eth0

DHCP Rogue server
spouštění nutné s administrátorskými právy:
sudo ./pds-dhcprogue -i eth1 -p 192.168.1.100-192.168.1.199 -g 192.168.1.1 -n 8.8.8.8 -d fit.vutbr.cz -l 600
