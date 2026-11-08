/** Generates a small, valid NanoPass C program for the Lab editor. */
export function generateNanoProgram(): string {
  const limit = 3 + Math.floor(Math.random() * 6);
  const scale = 2 + Math.floor(Math.random() * 7);
  const offset = Math.floor(Math.random() * 11);
  return `int transform(int value) {
  return value * ${scale} + ${offset};
}

int main() {
  int values[${limit}];
  int i = 0;
  int total = 0;

  while (i < ${limit}) {
    values[i] = transform(i);
    total = total + values[i];
    i = i + 1;
  }

  print(total);
  return total;
}`;
}
