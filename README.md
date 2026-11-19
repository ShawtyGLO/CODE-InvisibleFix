# InvisibleFix

Plugin de réparation vocale en C++ / JUCE : de-reverb, gate, nettoyage, traitement de pièce et
air. le premier plugin que j'ai crée.

## Ce qu'il fait

Une chaîne pensée pour récupérer une prise de voix imparfaite : couper ce qu'il y a entre les phrases (bruits statiques, respirations, bruits de bouche, etc.), atténuer la résonance de la pièce, retirer la réverbération de la voix, nettoyer la voix des sibilances et plosives, puis redonner de l'air en haut
du spectre. L'ordre des traitements a été pensé exprès pour fonctionner le plus efficacement.

## Pourquoi l'ordre compte

Chaque étage modifie ce que le suivant croit voir. Nettoyer avant de dé-réverbérer ne donne pas
le même résultat que l'inverse : la réverbération d'une pièce ressemble à du bruit pour un gate,
et une sibilance déjà atténuée ne se détecte plus de la même façon. C'est la première chose que
ce plugin m'a apprise, et c'est ce qui a demandé le plus d'essais.

## Construire

Ouvrir `InvisibleFix.jucer` dans Projucer, puis compiler sous Visual Studio 2022.
Cible : VST3, Windows 64 bits. Validation avec [pluginval](https://github.com/Tracktion/pluginval).

## Note

Conception, architecture DSP et validation sont de moi. L'écriture du code s'appuie sur une
assistance IA.

## Droits

© Lucas Lallier-Ginoux. **Tous droits réservés.** Dépôt publié en consultation uniquement,
comme portfolio. Aucune licence d'utilisation, de modification, de redistribution ou
d'exploitation commerciale n'est accordée.
