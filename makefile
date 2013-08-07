# Cursnake!

main : cursnake.c
	gcc -lpanel -lncurses -lform cursnake.c -o snake


commit : .git
	git commit -a && git push

clean :
	rm snake HiScore *~
