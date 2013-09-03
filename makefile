# Cursnake!

main : cursnake.c
	@cc -lpanel -lncurses -lform cursnake.c -o snake

run : cursnake.c snake
	@./snake

commit : .git
	@git commit -a && git push
