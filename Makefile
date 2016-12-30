EXE = run
SRC = $(wildcard *.cpp)
OBJ = $(addprefix build/,$(SRC:.cpp=.o))

all: $(EXE)

$(EXE): $(OBJ)
	@echo 'Building target: $(EXE)'
	g++ -std=c++14 -o $(EXE) $(OBJ)
	@echo 'Finished building target: $(EXE)'
	@echo ' '

build/%.o: %.cpp
	@echo 'Building file: $<'
	g++ -std=c++14 -Wall -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

clean:
	rm -f build/*.o $(EXE)*
