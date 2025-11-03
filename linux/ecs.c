#include <stdio.h>
#include <stdlib.h>

// ============ COMPONENTS (Just Data) ============
typedef struct {
    float x, y;
} Position;

typedef struct {
    float vx, vy;
} Velocity;

typedef struct {
    char symbol;
} Renderable;

// ============ ECS WORLD ============
#define MAX_ENTITIES 100

typedef struct {
    // Parallel arrays - index = entity ID
    Position positions[MAX_ENTITIES];
    Velocity velocities[MAX_ENTITIES];
    Renderable renderables[MAX_ENTITIES];
    
    // Bitmasks: which entities have which components
    int hasPosition[MAX_ENTITIES];
    int hasVelocity[MAX_ENTITIES];
    int hasRenderable[MAX_ENTITIES];
    
    int entityCount;
} World;

// ============ SYSTEMS (Logic) ============

// Physics System: updates Position using Velocity
void physicsSystem(World* w, float dt) {
    for (int i = 0; i < w->entityCount; i++) {
        if (w->hasPosition[i] && w->hasVelocity[i]) {
            // Move entity
            w->positions[i].x += w->velocities[i].vx * dt;
            w->positions[i].y += w->velocities[i].vy * dt;
            
            // Bounce off walls
            if (w->positions[i].x < 0 || w->positions[i].x > 79) {
                w->velocities[i].vx *= -2;
            }
            if (w->positions[i].y < 0 || w->positions[i].y > 23) {
                w->velocities[i].vy *= -2;
            }
        }
    }
}

// Render System: draws entities with Position + Renderable
void renderSystem(World* w) {
    // Clear screen
    printf("\033[2J\033[H");
    
    // Create screen buffer
    char screen[24][80];
    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 80; x++) {
            screen[y][x] = ' ';
        }
    }
    
    // Draw entities
    for (int i = 0; i < w->entityCount; i++) {
        if (w->hasPosition[i] && w->hasRenderable[i]) {
            int x = (int)w->positions[i].x;
            int y = (int)w->positions[i].y;
            if (x >= 0 && x < 80 && y >= 0 && y < 24) {
                screen[y][x] = w->renderables[i].symbol;
            }
        }
    }
    
    // Print screen
    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 80; x++) {
            putchar(screen[y][x]);
        }
        putchar('\n');
    }
}

// ============ ENTITY CREATION ============

int createEntity(World* w) {
    return w->entityCount++;
}

void addPosition(World* w, int entity, float x, float y) {
    w->positions[entity].x = x;
    w->positions[entity].y = y;
    w->hasPosition[entity] = 1;
}

void addVelocity(World* w, int entity, float vx, float vy) {
    w->velocities[entity].vx = vx;
    w->velocities[entity].vy = vy;
    w->hasVelocity[entity] = 1;
}

void addRenderable(World* w, int entity, char symbol) {
    w->renderables[entity].symbol = symbol;
    w->hasRenderable[entity] = 1;
}

// ============ MAIN ============
int main() {
    World world = {0};
    
    // Entity 0: Moving ball
    int ball = createEntity(&world);
    addPosition(&world, ball, 40.0f, 12.0f);
    addVelocity(&world, ball, 2.0f, 1.5f);
    addRenderable(&world, ball, 'O');
    
    // Entity 1: Static marker
    int marker = createEntity(&world);
    addPosition(&world, marker, 10.0f, 5.0f);
    addRenderable(&world, marker, 'X');
    // No velocity = doesn't move!
    
    // Entity 2: Invisible moving thing
    int ghost = createEntity(&world);
    addPosition(&world, ghost, 20.0f, 10.0f);
    addVelocity(&world, ghost, -1.0f, 0.5f);
    // No renderable = invisible!
    
    // Game loop
    for (int frame = 0; frame < 200; frame++) {
        physicsSystem(&world, 0.1f);  // Update physics
        renderSystem(&world);          // Draw
        
        // Simple delay
        for (int i = 0; i < 10000000; i++);
    }
    
    return 0;
}
