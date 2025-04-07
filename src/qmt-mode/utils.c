void qt_write_logs(double result_time, enum LOG_TAG tag)
{
	FILE *file = NULL;

	file = fopen(QT_LOG_FILE_PATH, "a");
	const char *log_tag_str = log_tag_to_str(tag);

	if (file) {
		fprintf(file, "%s %.6f\n", log_tag_str, result_time);
		fclose(file);
	} else {
		fputs("Error: could not open timing results file for appending.\n", stderr);
	}

	return;
}
