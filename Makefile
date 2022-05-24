goal: 
	gcc survey.c -l ncurses -l xml2 -I/usr/include/libxml2 -o main
	echo "Bulding... main"
test:
	./main -s sample.xml
validate:
	xmllint --schema schema.xsd sample.xml --noout
