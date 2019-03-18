# hot_potato_game
This is a model game which has a ringmaster and a group of player. The players are connected as a ring.   
At the beginning of the game, the ringmaster will create a potato, and deliver it randomly to one of the players. In each hop, the player who has the potato will deliver it to one of his neighbors randomly. After a number of hops, the game will end. The potato will be delivered back to the ringmaster. The potato will trace the path of players it passes.   
To start this game:   
make all  
ringmaster <port_num> <num_players> <num_hops>  
on each vm, run:   
player <machine_name> <port_num>   
