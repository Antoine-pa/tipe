CC=gcc
CFLAGS=-Wall -Wextra -g -O2
LDFLAGS=-lglfw -lGL -lGLEW -lGLU -lm

# Fichiers source et objets
SRC_DIR=src
BUILD_DIR=build
BIN_DIR=bin

# Sources et objets
SRCS=$(SRC_DIR)/shadertoy.c $(SRC_DIR)/kdtree.c $(SRC_DIR)/objects.c
OBJS=$(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TARGET=$(BIN_DIR)/shadertoy.bin

# Règle principale
all: directories $(TARGET)

# Création des répertoires si nécessaires
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BIN_DIR)

# Règle pour créer l'exécutable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Règle pour compiler les fichiers source en objets
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Dépendances spécifiques
$(BUILD_DIR)/shadertoy.o: $(SRC_DIR)/shadertoy.c $(SRC_DIR)/shadertoy.h $(SRC_DIR)/kdtree.h $(SRC_DIR)/objects.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kdtree.o: $(SRC_DIR)/kdtree.c $(SRC_DIR)/kdtree.h $(SRC_DIR)/objects.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/objects.o: $(SRC_DIR)/objects.c $(SRC_DIR)/objects.h
	$(CC) $(CFLAGS) -c $< -o $@

# Règle pour nettoyer le projet
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Règle pour exécuter le programme
run: all
	./$(TARGET) 0 0 0

# Règle pour exécuter avec KD-tree activé
run-kdtree: all
	./$(TARGET) 0 0 1

.PHONY: all clean directories run run-kdtree