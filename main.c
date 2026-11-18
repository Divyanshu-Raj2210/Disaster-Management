#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME 50
#define DEFAULT_TRUCK_CAPACITY 100

typedef struct {
    int id;
    char name[MAX_NAME];
    int quantity;
    int expiryDays; /* 0 means expired */
} Item;

typedef struct {
    int campId;
    char campName[MAX_NAME];
    int itemId;
    int quantity;
    int priority; /* 1 = high, 2 = medium, 3 = low */
} Request;

typedef struct {
    char campName[MAX_NAME];
    char itemName[MAX_NAME];
    int quantity;
    int truckNo;
} Allocation;

static Item *inventory = NULL;
static Request *requests = NULL;
static Allocation *allocations = NULL;
static int itemCount = 0, requestCount = 0, allocationCount = 0;
static int truckCapacity = DEFAULT_TRUCK_CAPACITY;

static int findItem(int id) {
    for (int i = 0; i < itemCount; i++)
        if (inventory[i].id == id) return i;
    return -1;
}

static int addAllocationRecord(const char *camp, const char *item,
                               int quantity, int truckNo) {
    Allocation *temp = realloc(allocations,
        (allocationCount + 1) * sizeof(*allocations));
    if (temp == NULL) return 0;

    allocations = temp;
    snprintf(allocations[allocationCount].campName, MAX_NAME, "%s", camp);
    snprintf(allocations[allocationCount].itemName, MAX_NAME, "%s", item);
    allocations[allocationCount].quantity = quantity;
    allocations[allocationCount].truckNo = truckNo;
    allocationCount++;
    return 1;
}

static void resetAllocations(void) {
    free(allocations);
    allocations = NULL;
    allocationCount = 0;
}

static int addDonation(void) {
    Item item;

    printf("\nEnter item ID: ");
    if (scanf("%d", &item.id) != 1) return 0;
    if (findItem(item.id) != -1) {
        printf("That item ID already exists. Use a new ID.\n");
        return 0;
    }

    printf("Enter item name: ");
    scanf(" %49[^\n]", item.name);
    printf("Enter quantity: ");
    if (scanf("%d", &item.quantity) != 1 || item.quantity <= 0) {
        printf("Quantity must be greater than zero.\n");
        return 0;
    }
    printf("Enter days until expiry (0 = expired): ");
    if (scanf("%d", &item.expiryDays) != 1 || item.expiryDays < 0) {
        printf("Expiry days cannot be negative.\n");
        return 0;
    }

    Item *temp = realloc(inventory, (itemCount + 1) * sizeof(*inventory));
    if (temp == NULL) {
        printf("Memory allocation failed.\n");
        return 0;
    }
    inventory = temp;
    inventory[itemCount++] = item;
    printf("Donation added successfully.\n");
    return 1;
}

static int addRequest(void) {
    Request request;

    printf("\nEnter camp ID: ");
    if (scanf("%d", &request.campId) != 1) return 0;
    printf("Enter camp name: ");
    scanf(" %49[^\n]", request.campName);
    printf("Enter requested item ID: ");
    if (scanf("%d", &request.itemId) != 1) return 0;
    printf("Enter required quantity: ");
    if (scanf("%d", &request.quantity) != 1 || request.quantity <= 0) {
        printf("Quantity must be greater than zero.\n");
        return 0;
    }
    printf("Enter priority (1=High, 2=Medium, 3=Low): ");
    if (scanf("%d", &request.priority) != 1 ||
        request.priority < 1 || request.priority > 3) {
        printf("Priority must be 1, 2, or 3.\n");
        return 0;
    }

    Request *temp = realloc(requests, (requestCount + 1) * sizeof(*requests));
    if (temp == NULL) {
        printf("Memory allocation failed.\n");
        return 0;
    }
    requests = temp;
    requests[requestCount++] = request;

    if (findItem(request.itemId) == -1)
        printf("Request saved. No matching inventory item exists yet.\n");
    else
        printf("Camp request added successfully.\n");
    return 1;
}

static void displayInventory(void) {
    printf("\n--- INVENTORY ---\n");
    if (itemCount == 0) {
        printf("No inventory records.\n");
        return;
    }
    printf("%-8s %-24s %-12s %-12s\n", "ID", "ITEM", "QUANTITY", "EXPIRY");
    for (int i = 0; i < itemCount; i++) {
        printf("%-8d %-24s %-12d %-12d %s\n",
            inventory[i].id, inventory[i].name,
            inventory[i].quantity, inventory[i].expiryDays,
            inventory[i].expiryDays == 0 ? "EXPIRED" : "");
    }
}

static void displayRequests(void) {
    printf("\n--- CAMP REQUESTS ---\n");
    if (requestCount == 0) {
        printf("No camp requests.\n");
        return;
    }
    for (int i = 0; i < requestCount; i++) {
        printf("Camp: %s | Item ID: %d | Quantity: %d | Priority: %d\n",
            requests[i].campName, requests[i].itemId,
            requests[i].quantity, requests[i].priority);
    }
}

static int findUsableStock(int itemId, int earliestExpiryFirst) {
    int best = -1;
    for (int i = 0; i < itemCount; i++) {
        if (inventory[i].id != itemId ||
            inventory[i].quantity <= 0 ||
            inventory[i].expiryDays <= 0)
            continue;

        if (best == -1 ||
            (earliestExpiryFirst &&
             inventory[i].expiryDays < inventory[best].expiryDays))
            best = i;
    }
    return best;
}

static int allocateQuantity(int requestIndex, int amount,
                            int *truckNo, int *remainingCapacity,
                            int earliestExpiryFirst) {
    int remaining = amount;

    while (remaining > 0) {
        int stockIndex = findUsableStock(requests[requestIndex].itemId,
                                         earliestExpiryFirst);
        if (stockIndex == -1) break;

        if (*remainingCapacity == 0) {
            (*truckNo)++;
            *remainingCapacity = truckCapacity;
        }

        int part = remaining;
        if (part > inventory[stockIndex].quantity)
            part = inventory[stockIndex].quantity;
        if (part > *remainingCapacity)
            part = *remainingCapacity;

        if (!addAllocationRecord(requests[requestIndex].campName,
                                 inventory[stockIndex].name, part, *truckNo)) {
            printf("Could not save allocation: memory allocation failed.\n");
            return amount - remaining;
        }

        inventory[stockIndex].quantity -= part;
        *remainingCapacity -= part;
        remaining -= part;
    }
    return amount - remaining;
}

static void sortRequestOrder(int *order) {
    for (int i = 0; i < requestCount; i++) order[i] = i;
    for (int i = 0; i < requestCount; i++) {
        for (int j = i + 1; j < requestCount; j++) {
            if (requests[order[j]].priority < requests[order[i]].priority) {
                int temp = order[i];
                order[i] = order[j];
                order[j] = temp;
            }
        }
    }
}

static void runGreedy(int *order) {
    int truckNo = 1, capacityLeft = truckCapacity;
    sortRequestOrder(order);
    for (int i = 0; i < requestCount; i++)
        allocateQuantity(order[i], requests[order[i]].quantity,
                         &truckNo, &capacityLeft, 0);
}

static void runProportional(int *order) {
    int truckNo = 1, capacityLeft = truckCapacity;
    int *targets = calloc((size_t)requestCount, sizeof(*targets));
    if (requestCount > 0 && targets == NULL) {
        printf("Memory allocation failed.\n");
        return;
    }

    /* Calculate each request's share from the original available stock. */
    for (int r = 0; r < requestCount; r++) {
        int supply = 0, demand = 0;
        for (int i = 0; i < itemCount; i++)
            if (inventory[i].id == requests[r].itemId &&
                inventory[i].expiryDays > 0)
                supply += inventory[i].quantity;
        for (int j = 0; j < requestCount; j++)
            if (requests[j].itemId == requests[r].itemId)
                demand += requests[j].quantity;

        if (demand > 0) {
            targets[r] = (int)((long long)requests[r].quantity * supply / demand);
            if (targets[r] > requests[r].quantity)
                targets[r] = requests[r].quantity;
        }
    }

    for (int r = 0; r < requestCount; r++)
        allocateQuantity(r, targets[r], &truckNo, &capacityLeft, 0);

    free(targets);
    (void)order;
}

static void runConstraintBased(int *order) {
    int truckNo = 1, capacityLeft = truckCapacity;
    sortRequestOrder(order);
    for (int i = 0; i < requestCount; i++)
        allocateQuantity(order[i], requests[order[i]].quantity,
                         &truckNo, &capacityLeft, 1);
}

static int calculateScore(void) {
    int score = 0;
    for (int r = 0; r < requestCount; r++) {
        int allocated = 0;
        int itemIndex = findItem(requests[r].itemId);
        if (itemIndex == -1) {
            score += requests[r].quantity * requests[r].priority;
            continue;
        }

        for (int a = 0; a < allocationCount; a++) {
            if (strcmp(allocations[a].campName, requests[r].campName) == 0 &&
                strcmp(allocations[a].itemName, inventory[itemIndex].name) == 0)
                allocated += allocations[a].quantity;
        }

        int unmet = requests[r].quantity - allocated;
        if (unmet > 0) score += unmet * requests[r].priority;
    }
    return score;
}

static void allocationReport(void) {
    printf("\n--- TRUCK-WISE ALLOCATION MANIFEST ---\n");
    if (allocationCount == 0) {
        printf("No allocations have been made.\n");
        return;
    }
    for (int a = 0; a < allocationCount; a++) {
        printf("Truck %d | Camp: %s | Item: %s | Quantity: %d\n",
            allocations[a].truckNo, allocations[a].campName,
            allocations[a].itemName, allocations[a].quantity);
    }
}

static void unmetNeedsReport(void) {
    printf("\n--- UNMET NEEDS ---\n");
    for (int r = 0; r < requestCount; r++) {
        int allocated = 0;
        int itemIndex = findItem(requests[r].itemId);
        if (itemIndex != -1) {
            for (int a = 0; a < allocationCount; a++) {
                if (strcmp(allocations[a].campName, requests[r].campName) == 0 &&
                    strcmp(allocations[a].itemName, inventory[itemIndex].name) == 0)
                    allocated += allocations[a].quantity;
            }
        }
        int unmet = requests[r].quantity - allocated;
        if (unmet > 0)
            printf("Camp: %s | Item ID: %d | Unmet quantity: %d\n",
                requests[r].campName, requests[r].itemId, unmet);
    }
}

static void leftoverReport(void) {
    printf("\n--- LEFTOVER / EXPIRED INVENTORY ---\n");
    for (int i = 0; i < itemCount; i++) {
        if (inventory[i].expiryDays == 0)
            printf("%s | EXPIRED | Quantity: %d\n",
                   inventory[i].name, inventory[i].quantity);
        else if (inventory[i].quantity > 0)
            printf("%s | Remaining quantity: %d\n",
                   inventory[i].name, inventory[i].quantity);
    }
}

static void executeStrategy(int strategy, int showReports) {
    int *order = malloc((size_t)requestCount * sizeof(*order));
    if (requestCount > 0 && order == NULL) {
        printf("Memory allocation failed.\n");
        return;
    }

    resetAllocations();
    if (strategy == 1) runGreedy(order);
    else if (strategy == 2) runProportional(order);
    else runConstraintBased(order);

    free(order);

    if (showReports) {
        allocationReport();
        unmetNeedsReport();
        leftoverReport();
    }
}

static void optimizeAllocation(void) {
    Item *original = NULL;
    if (itemCount > 0) {
        original = malloc((size_t)itemCount * sizeof(*original));
        if (original == NULL) {
            printf("Memory allocation failed.\n");
            return;
        }
        memcpy(original, inventory, (size_t)itemCount * sizeof(*original));
    }

    int bestStrategy = 1, bestScore = 0;
    for (int strategy = 1; strategy <= 3; strategy++) {
        if (itemCount > 0)
            memcpy(inventory, original, (size_t)itemCount * sizeof(*original));

        executeStrategy(strategy, 0);
        int score = calculateScore();
        printf("Strategy %d score: %d\n", strategy, score);

        if (strategy == 1 || score < bestScore) {
            bestScore = score;
            bestStrategy = strategy;
        }
    }

    if (itemCount > 0)
        memcpy(inventory, original, (size_t)itemCount * sizeof(*original));
    free(original);

    printf("\nBest strategy: %s\n",
        bestStrategy == 1 ? "Greedy by priority" :
        bestStrategy == 2 ? "Proportional distribution" :
                            "Constraint-based (perishable first)");
    printf("Best score: %d\n", bestScore);
    executeStrategy(bestStrategy, 1);
}

static void setTruckCapacity(void) {
    int capacity;
    printf("Enter truck capacity: ");
    if (scanf("%d", &capacity) != 1 || capacity <= 0) {
        printf("Invalid capacity. Keeping current capacity (%d).\n",
               truckCapacity);
        return;
    }
    truckCapacity = capacity;
    printf("Truck capacity updated to %d.\n", truckCapacity);
}

int main(void) {
    int choice;

    do {
        printf("\n========== DISASTER RELIEF RESOURCE PLANNER ==========\n");
        printf("1. Add donation\n");
        printf("2. Add camp request\n");
        printf("3. Display inventory\n");
        printf("4. Display camp requests\n");
        printf("5. Greedy allocation by priority\n");
        printf("6. Proportional allocation\n");
        printf("7. Constraint-based allocation\n");
        printf("8. Compare strategies / optimize\n");
        printf("9. Display allocation manifest\n");
        printf("10. Display unmet needs\n");
        printf("11. Display leftover / expired inventory\n");
        printf("12. Set truck capacity\n");
        printf("0. Exit\n");
        printf("Enter choice: ");

        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Please restart and enter a number.\n");
            break;
        }

        switch (choice) {
            case 1: addDonation(); break;
            case 2: addRequest(); break;
            case 3: displayInventory(); break;
            case 4: displayRequests(); break;
            case 5: executeStrategy(1, 1); break;
            case 6: executeStrategy(2, 1); break;
            case 7: executeStrategy(3, 1); break;
            case 8: optimizeAllocation(); break;
            case 9: allocationReport(); break;
            case 10: unmetNeedsReport(); break;
            case 11: leftoverReport(); break;
            case 12: setTruckCapacity(); break;
            case 0: printf("Thank you for using the planner.\n"); break;
            default: printf("Invalid choice. Try again.\n");
        }
    } while (choice != 0);

    free(inventory);
    free(requests);
    free(allocations);
    return 0;
}
