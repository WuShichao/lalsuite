#gdb --args ./InferenceTest --IFO [H1] --cache [LALLIGO] --PSDstart 864161757.0 --PSDlength 1000 --srate 1024 --seglen 10 --trigtime 864162757.0 --injXML test.xml
openmpirun -np 8 ./InferenceTest --IFO [H1] --cache [LALLIGO] --PSDstart 864161757.0 --PSDlength 1000 --srate 1024 --seglen 10 --trigtime 864162757.0 --injXML test.xml
#./InferenceTest --IFO [H1] --cache [H-H1_RDS_C03_L2-873739103-873739231.cache] --PSDstart 864161757.0 --PSDlength 1000 --srate 1024 --seglen 10 --trigtime 864162757.0 --injXML test.xml
