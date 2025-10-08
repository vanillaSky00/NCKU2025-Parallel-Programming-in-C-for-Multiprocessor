#include<stdio.h>
#include<string.h>

int main() {

    int numOfPassword;
    int numOfQuery;
    scanf("%d %d", &numOfPassword, &numOfQuery);
    
    static char passwords[100000][9];

    for (int i = 0; i < numOfPassword; i++) {
        scanf("%8s", passwords[i]); // read at mostt 8 char
    }
    
    int ans = 0;
    int MAX_TIME = 30 * numOfQuery;
    while (numOfQuery > 0) {
        numOfQuery--;
        int time;
        char guess[9];
        scanf("%d %8s", &time, guess);
        if(time <= 0 || time >= MAX_TIME) continue;
        
        int idx = (time - 1)/ 30;
        if (strcmp(guess, passwords[idx]) == 0) ans++;
    }

    printf("%d\n", ans);
    return 0;
}
