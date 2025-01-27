#include <iostream>
#include <cstdlib>
#include <ctime>
using namespace std;

int main()
{
    //Taking inputs
    int balls, slots;
    cout << "Enter the number of balls to drop: ";
    cin >> balls;
    cout << "Enter the number of slots in the bean machine: ";
    cin >> slots;

    srand(time(0));
    char path[50];
    for (int i = 0; i < balls; i++)
    {
        int curr_pos = slots/2;
        for (int j = 0; j < slots; j++)
        {
            if (curr_pos == 0)
            {
                path[j] = 'R';
                curr_pos++;
                continue;
            }
            else if (curr_pos == j + 1)
            {
                path[j] = 'L';
            }
            if (rand()%2)
            {
                if ()
            }
        }
    }



    return 0;
}