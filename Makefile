goal: 
	gcc -Wall -Wextra -g survey.c -l ncurses -l xml2 -I/usr/include/libxml2 -o main 2>&1 > makeReport
	echo "Bulding... main"
test:
	./main -s sample.xml
validate:
	xmllint --schema schema.xsd sample.xml --noout
