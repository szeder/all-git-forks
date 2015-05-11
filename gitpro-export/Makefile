NAME = std-to-csv

ANALYZER_PATH = analyzers
TRANSLATOR_PATH = translators

ANALYZER = csv-analyzer
TRANSLATOR = csv-translator

STD_API = std-api/standarization-API.h std-api/standarization-API.c

TRANSLATOR_IFACE = iface-translate

GENERIC_ANALYZER = generic-analyzer

TRANSFORM_ENGINE = transform-engine

std:
	flex $(ANALYZER_PATH)/$(ANALYZER).l
	gcc -c $(STD_API)
	gcc -c lex.yy.c
	gcc -o $(NAME) *.o -lfl

dest:
	flex $(GENERIC_ANALYZER).l
	bison -o translator.tab.c $(TRANSFORM_ENGINE).y -yd
	gcc -c lex.yy.c
	gcc -c $(TRANSLATOR_IFACE).h $(TRANSLATOR_IFACE).c $(TRANSLATOR_PATH)/$(TRANSLATOR).c
	gcc -c translator.tab.c
	gcc -o $(NAME) *.o -lfl -ly

clean:  
	rm *.o 
	rm lex.* 
	rm *.tab.*
	rm *.gch
