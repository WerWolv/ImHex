namespace hex::init {
    /**
     * determine which subcommand to run from the args given, and run it.
     * In some cases, the subcommand might exit the program (e.g. --help)
     * and so this function might not return
     */
    void processArguments(const std::vector<std::string> &args);

    void forwardSubCommand(const std::string &cmdName, const std::vector<std::string> &args);
}
