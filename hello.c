{
    int a;
    int b;
    int c;
    int arr[4];

    a = 3;
    b = 4;
    c = a + b * 2;

    arr[1] = c;
    a = arr[1];

    if (a < b && c == 10) {
        c = c - 1;
    }

    if (a > b || !(c == 10)) {
        c = c + 1;
    }

    if (!(a < b) && !(c != 10)) {
        c = c + 2;
    }
}
