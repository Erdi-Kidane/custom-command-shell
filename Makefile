#Name: Erdi Kidane                                                                                                                                                               
#Email: Erdi.Kidane@ucla.edu                                                                                                                                                     
#ID: 904951100                                                                                                                                                                    

default:
	gcc -Wall -Wextra -g -o simpsh customShell.c
clean:
	rm simpsh customShell.tar.gz
dist:
	tar -czvf customShell.tar.gz customShell.c Makefile README Report.pdf
check:
	echo "no tests required, see Report.pdf"
