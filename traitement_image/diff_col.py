#!/usr/bin/env python3

from PIL import Image
import numpy as np
import sys

def image_difference(image1_path, image2_path, output_path=None):
    """
    Calcule la différence entre deux images de même taille.
    Si les pixels sont identiques, le pixel résultant est gris.
    Sinon, c'est gris moins la différence entre les deux pixels.
    
    Args:
        image1_path: Chemin vers la première image
        image2_path: Chemin vers la deuxième image
        output_path: Chemin pour sauvegarder l'image résultante (optionnel)
        
    Returns:
        L'image résultante
    """
    # Charger les images
    try:
        img1 = Image.open(image1_path).convert('RGB')
        img2 = Image.open(image2_path).convert('RGB')
    except Exception as e:
        print(f"Erreur lors du chargement des images: {e}")
        return None
    
    # Vérifier que les images ont la même taille
    if img1.size != img2.size:
        print(f"Les images n'ont pas la même taille: {img1.size} vs {img2.size}")
        return None
    
    # Convertir en tableaux numpy pour faciliter les calculs
    arr1 = np.array(img1)
    arr2 = np.array(img2)
    
    # Créer une nouvelle image avec un fond gris
    gray_value = 128
    result = np.ones_like(arr1) * gray_value
    
    # Calculer la différence
    for i in range(arr1.shape[0]):
        for j in range(arr1.shape[1]):
            if np.array_equal(arr1[i, j], arr2[i, j]):
                # Les pixels sont identiques, laisser gris
                pass
            else:
                # Les pixels sont différents
                # Calculer la différence et soustraire du gris
                pixel_diff = np.abs(arr1[i, j].astype(np.int16) - arr2[i, j].astype(np.int16))
                # Moyenne des différences sur les 3 canaux RGB
                diff_mean = np.mean(pixel_diff)
                # Soustraire du gris (en assurant que la valeur reste positive)
                result[i, j] = np.clip(gray_value - diff_mean, 0, 255)
    
    # Convertir le résultat en image
    result_img = Image.fromarray(result.astype(np.uint8))
    
    # Sauvegarder l'image si un chemin de sortie est spécifié
    if output_path:
        result_img.save(output_path)
        print(f"Image de différence sauvegardée dans {output_path}")
    
    return result_img

if __name__ == "__main__":
    # Exemple d'utilisation en ligne de commande
    if len(sys.argv) < 3:
        print("Usage: python diff_col.py image1.jpg image2.jpg [output.jpg]")
        sys.exit(1)
    
    image1_path = sys.argv[1]
    image2_path = sys.argv[2]
    
    output_path = sys.argv[3] if len(sys.argv) > 3 else "difference.png"
    
    image_difference(image1_path, image2_path, output_path)
