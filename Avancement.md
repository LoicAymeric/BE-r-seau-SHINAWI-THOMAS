Dernière version : 
Version 4.1 (finie) 

Ce qui fonctionne : 
-Négociation entre la source et le puit (moyenne des deux taux voulu) 
-Phase de connection (avec un cond_wait pour le accept) 
-Gestion des pertes (calcul en temps réel du taux de pertes actuels + gestion du nombre de pertes consécutives) 


Problèmes : 
-On ne savait pas trop quoi faire pour le bind  mais ça fonctionne 
-Pas eu le temps de faire la 4.2, mais on ne comprennait pas non plus ce qu'il fallait faire

