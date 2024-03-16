release_notes_generator: release_notes_generator.cpp
	g++ -o release_notes_generator release_notes_generator.cpp -lcurl -I.