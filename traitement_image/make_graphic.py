#!/usr/bin/env python3

import numpy as np
import matplotlib.pyplot as plt
from PIL import Image
import sys

def create_grayscale_histogram(image_path, output_path=None):
    """
    Crée un histogramme montrant la distribution des niveaux de gris dans une image.
    La valeur du milieu (128) est exclue de l'histogramme.
    
    Args:
        image_path: Chemin vers l'image à analyser
        output_path: Chemin pour sauvegarder l'histogramme (optionnel)
        
    Returns:
        Affiche l'histogramme et le sauvegarde si output_path est spécifié
    """
    try:
        # Charger l'image et la convertir en niveaux de gris
        img = Image.open(image_path).convert('L')
    except Exception as e:
        print(f"Erreur lors du chargement de l'image: {e}")
        return None
    
    # Convertir en tableau numpy
    img_array = np.array(img)
    
    # Nombre total de pixels (excluant les pixels de valeur 128)
    middle_gray = 128
    mask = img_array != middle_gray
    filtered_array = img_array[mask]
    total_pixels = filtered_array.size
    
    # Calculer l'histogramme en excluant la valeur du milieu
    hist = np.zeros(256)
    for i in range(256):
        if i != middle_gray:
            count = np.sum(filtered_array == i)
            hist[i] = count
    
    # Convertir en pourcentage
    hist_percent = hist * 100.0 / total_pixels
    
    # Créer le graphique
    plt.figure(figsize=(10, 6))
    plt.bar(range(256), hist_percent, width=1.0, color='gray')
    
    plt.xlabel('Niveau de gris (0-255)')
    plt.ylabel('Pourcentage de pixels (%)')
    plt.title('Distribution des niveaux de gris')
    plt.xlim([0, 255])
    plt.grid(alpha=0.3)
    
    # Afficher des informations supplémentaires
    median_val = np.median(filtered_array)
    mean_val = np.mean(filtered_array)
    plt.axvline(x=median_val, color='red', linestyle='--', label=f'Médiane: {median_val:.1f}')
    plt.axvline(x=mean_val, color='blue', linestyle='--', label=f'Moyenne: {mean_val:.1f}')
    plt.legend()
    
    # Sauvegarder l'histogramme si un chemin est spécifié
    if output_path:
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        print(f"Histogramme sauvegardé dans {output_path}")
    
    # Afficher l'histogramme
    plt.show()
    
    return hist_percent

if __name__ == "__main__":
    # Exemple d'utilisation en ligne de commande
    if len(sys.argv) < 2:
        print("Usage: python make_graphic.py image.jpg [output_histogram.png]")
        sys.exit(1)
    
    image_path = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else None
    
    create_grayscale_histogram(image_path, output_path)
