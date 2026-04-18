#ifndef TUNER_H
#define TUNER_H

// Run the Texel tuner
// data_file: path to .epd file with "FEN \"result\"" lines
// epochs: number of SGD iterations
// lr: learning rate
// lambda: L2 regularization strength
// batch: mini-batch size
void tuner_run(const char *data_file, int epochs, double lr, double lambda, int batch);

#endif // TUNER_H
