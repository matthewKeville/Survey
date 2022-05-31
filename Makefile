goal: 
	gcc -Wall -Wextra -g survey.c -l ncurses -l xml2 -I/usr/include/libxml2 -o main 2>&1 > makeReport
	echo "Bulding... main"
#run the application against sample input
test:
	./main -s small-sample.xml
#check that the sample xml is valid
validate:
	xmllint --schema schema.xsd sample.xml --noout
#check for memory errors against sample input, place and launch into a vim buffer
memcheck:
	valgrind ./main -s sample.xml 2>&1 | vim - -R
