# InvisibleFix

Plugin de réparation vocale en C++ / JUCE : de-reverb, gate, nettoyage, traitement de pièce et
air. Le plus avancé de la gamme Invisible, et le premier plugin que j'ai écrit.

## Ce qu'il fait

Une chaîne pensée pour récupérer une prise de voix imparfaite : retirer la réverbération de la
pièce, couper ce qu'il y a entre les phrases, nettoyer le grain, puis redonner de l'air en haut
du spectre. L'ordre des étages est le sujet du projet autant que les étages eux-mêmes.

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
