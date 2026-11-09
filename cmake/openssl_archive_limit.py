
from pathlib import Path
import sys

path = Path(sys.argv[1])
lines = path.read_text().splitlines(keepends=True)
output = []
for line in lines:
    prefix = '\t$(AR) $(ARFLAGS) '
    if line.startswith(prefix):
        library, *objects = line[len(prefix):].split()
        for start in range(0, len(objects), 100):
            output.append(prefix + library + ' ' + ' '.join(objects[start:start + 100]) + '\n')
    else:
        output.append(line)
updated = ''.join(output)
if updated != ''.join(lines):
    path.write_text(updated)
