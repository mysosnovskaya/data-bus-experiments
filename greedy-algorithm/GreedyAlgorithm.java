import javafx.util.Pair;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.Set;
import java.util.stream.Collectors;

public class GreedyAlgorithm {
    private static String FILE_NAME_PATTERN = "([^\\d]*)(\\d+.*.txt)";
    private static String DATA_BUS_RESULT_PATH;
    private static String EXAMPLES_PATH;
    private static Integer CORES_COUNT;
    private static String OUTPUT_DIR;

    // jobIndex, standardTime, busPercent,
    private static Map<String, Pair<Integer, Double>> data = new HashMap<>();

    private static int coresCount;
    private static int jobsCount;
    private static int[] tactsToExecute;
    private static double[] busPercent;

    private static List<List<Integer>> schedule = new LinkedList<>();
    private static Map<Integer, List<Integer>> keyJobShouldBeAfterListJobs = new HashMap<>();
    private static Map<Integer, String> jobIndexToJobIdMap = new HashMap<>();

    private static void fillDataBusMap() throws IOException {
        try (BufferedReader br = new BufferedReader(new FileReader(DATA_BUS_RESULT_PATH))) {
            String strLine;
            while ((strLine = br.readLine()) != null) {
                String[] splitedData = strLine.trim().split(" ");
                data.put(splitedData[0].replace("_", " "), new Pair<>(Integer.parseInt(splitedData[1]), Double.parseDouble(splitedData[2])));
            }
        }
    }

    private static void prepareData(File file) throws IOException {
        String fileName = file.getName();
        if (!fileName.endsWith(".txt")) {
            return;
        }
        System.out.println("start to execute: " + fileName);
        schedule = new LinkedList<>();
        keyJobShouldBeAfterListJobs = new HashMap<>();
        jobIndexToJobIdMap = new HashMap<>();
        coresCount = CORES_COUNT;

        try (BufferedReader br = new BufferedReader(new FileReader(file))) {
            jobsCount = Integer.parseInt(br.readLine());
            tactsToExecute = new int[jobsCount];
            busPercent = new double[jobsCount];
            var jobs = br.readLine().split("  ");
            for (int j = 0; j < jobsCount; j++) {
                String jobId = jobs[j];
                jobIndexToJobIdMap.put(j, jobId);
                tactsToExecute[j] = data.get(jobId).getKey();
                busPercent[j] = data.get(jobId).getValue();
                keyJobShouldBeAfterListJobs.put(j, new LinkedList<>());
            }
            String strLine;
            int[][] order = new int[jobsCount][jobsCount];
            int row = 0;
            while (!Objects.equals(strLine = br.readLine(), "")) {
                String[] values = strLine.split(" ");
                for (int i = 0; i < jobsCount; i++) {
                    order[row][i] = Integer.parseInt(values[i]);
                }
                row++;
            }

            for (int i = 0; i < jobsCount; i++) {
                for (int j = 0; j < jobsCount; j++) {
                    if (order[i][j] == 1) {
                        keyJobShouldBeAfterListJobs.get(i).add(j);
                    }
                }
            }
        }
    }

    public static void main(String[] args) throws IOException {
        for (int i = 0; i < args.length; i++) {
            String paramName = args[i];

            if (Objects.equals(paramName, "--cores-count")) {
                i++;
                CORES_COUNT = Integer.parseInt(args[i]);
            }

            if (Objects.equals(paramName, "--data-bus-file")) {
                i++;
                DATA_BUS_RESULT_PATH = args[i];
            }

            if (Objects.equals(paramName, "--path-to-examples")) {
                i++;
                EXAMPLES_PATH = args[i];
            }

            if (Objects.equals(paramName, "--output-dir")) {
                i++;
                OUTPUT_DIR = args[i];
            }
        }

        if (CORES_COUNT == null) {
            throw new RuntimeException("Missing cores-count. See README.md for more information");
        }
        if (DATA_BUS_RESULT_PATH == null) {
            throw new RuntimeException("Missing data-bus-file. See README.md for more information");
        }
        if (EXAMPLES_PATH == null) {
            throw new RuntimeException("Missing path-to-examples. See README.md for more information");
        }
        if (OUTPUT_DIR == null) {
            throw new RuntimeException("Missing output-dir. See README.md for more information");
        }

        fillDataBusMap();

        List<Path> paths = Files.walk(Paths.get(EXAMPLES_PATH)).collect(Collectors.toList());
        for (Path path : paths) {
            File file = path.toFile();
            prepareData(file);

            double estimateScheduleTime = buildSchedule();

            writeToFile(file.getName(), getOrderMatrix(), estimateScheduleTime);
        }
    }

    private static double buildSchedule() {
        List<Integer> notExecutedJodIndexes = new LinkedList<>();
        List<Integer> tactsToFullExecution = new LinkedList<>();
        for (int j = 0; j < jobsCount; j++) {
            notExecutedJodIndexes.add(j);
            tactsToFullExecution.add(tactsToExecute[j]);
        }

        List<Integer> firstMode = getNextMode(100, coresCount, new LinkedList<>(notExecutedJodIndexes), notExecutedJodIndexes);
        schedule.add(firstMode);

        double estimatedExecutionDuration = estimateModeDuration(firstMode, notExecutedJodIndexes, tactsToFullExecution);
        while (!notExecutedJodIndexes.isEmpty()) {
            int freeBusPercent = 100;
            Set<Integer> nextMode = new HashSet<>();
            // в новой моде продолжаем выполнять те работы, которые начались в предыдущей моде
            for (Integer jobIndex : schedule.get(schedule.size() - 1)) {
                if (notExecutedJodIndexes.contains(jobIndex)) {
                    nextMode.add(jobIndex);
                    freeBusPercent -= busPercent[jobIndex];
                }
            }
            List<Integer> jobsToChooseFrom = new LinkedList<>(notExecutedJodIndexes);
            jobsToChooseFrom.removeAll(nextMode);
            nextMode.addAll(getNextMode(freeBusPercent, coresCount - nextMode.size(), jobsToChooseFrom, notExecutedJodIndexes));
            schedule.add(new LinkedList<>(nextMode));
            double estimatedModeDuration = estimateModeDuration(schedule.get(schedule.size() - 1), notExecutedJodIndexes, tactsToFullExecution);
            estimatedExecutionDuration += estimatedModeDuration;
        }
        return estimatedExecutionDuration;
    }

    private static List<Integer> getNextMode(int freeBusPercent, int freeCoresCount, List<Integer> jobsToChooseFrom,
                                             List<Integer> notExecutedJodIndexes) {
        List<Integer> mode = new LinkedList<>();
        for (int i = 0; i < freeCoresCount && !jobsToChooseFrom.isEmpty(); i++) {
            Integer jobIndexToExecute;
            if (freeBusPercent <= 0) {
                jobIndexToExecute = getJobIndexWithMinBusPercent(jobsToChooseFrom, notExecutedJodIndexes);
            } else {
                jobIndexToExecute = findClosestJobToRemainingFreeBus(freeBusPercent / (freeCoresCount - i), jobsToChooseFrom, notExecutedJodIndexes);
            }
            if (jobIndexToExecute != null) {
                jobsToChooseFrom.remove(jobIndexToExecute);
                freeBusPercent -= busPercent[jobIndexToExecute];
                mode.add(jobIndexToExecute);
            } else {
                return mode;
            }
        }
        return mode;
    }

    private static Integer getJobIndexWithMinBusPercent(List<Integer> jobsToChooseFrom, List<Integer> notExecutedJodIndexes) {
        Integer jobIndex = null;
        for (int i = 0; i < jobsToChooseFrom.size(); i++) {
            if (busPercent[jobsToChooseFrom.get(i)] < Optional.ofNullable(jobIndex).map(ind -> busPercent[jobsToChooseFrom.get(ind)]).orElse(100.0) &&
                    keyJobShouldBeAfterListJobs.get(jobsToChooseFrom.get(i)).stream().noneMatch(notExecutedJodIndexes::contains)) {
                jobIndex = i;
            }
        }
        return Optional.ofNullable(jobIndex).map(jobsToChooseFrom::get).orElse(null);
    }

    private static Integer findClosestJobToRemainingFreeBus(int percent, List<Integer> jobsToChooseFrom, List<Integer> notExecutedJodIndexes) {
        Integer closestJobIndex = null;
        for (int i = 0; i < jobsToChooseFrom.size(); i++) {
            if (Math.abs(percent - busPercent[jobsToChooseFrom.get(i)])
                    <= Optional.ofNullable(closestJobIndex).map(ind -> Math.abs(percent - busPercent[notExecutedJodIndexes.get(ind)]))
                            .orElse(100.0)
                    && keyJobShouldBeAfterListJobs.get(jobsToChooseFrom.get(i)).stream().noneMatch(notExecutedJodIndexes::contains)) {
                closestJobIndex = i;
            }
        }
        return Optional.ofNullable(closestJobIndex).map(jobsToChooseFrom::get).orElse(null);
    }

    private static double estimateModeDuration(List<Integer> mode, List<Integer> notExecutedJodIndexes, List<Integer> tactsToFullExecution) {
        List<GAJob> sortedByBusPercent = mode.stream().map(index -> new GAJob(busPercent[index], index))
                .sorted(Comparator.comparingDouble(j -> j.busPercent)).collect(Collectors.toList());

        int freeBusPercent = 100;
        int leftFreeCoresCount = mode.size();

        Map<Integer, Double> indexJobToVelocity = new HashMap<>();
        for (GAJob gaJob : sortedByBusPercent) {
            // TODO here was integer / integer
            if (gaJob.busPercent <= ((double) freeBusPercent) / leftFreeCoresCount) {
                indexJobToVelocity.put(gaJob.jobIndex, 1.0);
                freeBusPercent -= gaJob.busPercent;
            } else {
                int busPercentForJob = freeBusPercent / leftFreeCoresCount;
                double velocity = ((double) busPercentForJob) / gaJob.busPercent;
                indexJobToVelocity.put(gaJob.jobIndex, velocity);
                freeBusPercent -= busPercentForJob;
            }
            leftFreeCoresCount--;
        }

        double modeTime = Double.MAX_VALUE;
        // определяем, какая работа завершится первой. это событие и будет окончанием моды, т.к. придется выбирать новую моду
        for (Integer integer : mode) {
            double timeToFullExecution = tactsToFullExecution.get(integer) / indexJobToVelocity.get(integer);
            if (modeTime >= timeToFullExecution) {
                modeTime = timeToFullExecution;
            }
        }

        for (Integer job : mode) {
            Integer leftTactsToExecute = tactsToFullExecution.get(job);
            int executedInThisModeTacts = ((int) (modeTime * indexJobToVelocity.get(job)));
            int leftExecuteAfterThisMode = leftTactsToExecute - executedInThisModeTacts;
            tactsToFullExecution.set(job, Math.max(leftExecuteAfterThisMode, 0));
            if (leftExecuteAfterThisMode <= 1) {
                notExecutedJodIndexes.remove(job);
            }
        }

        return modeTime;
    }

    private static int[][] getOrderMatrix() {
        int[][] orderMatrix = new int[jobsCount][jobsCount];

        // mode number, jobsToStart, endedJobs
        Map<Integer, Pair<List<Integer>, List<Integer>>> modes = new HashMap<>();
        modes.put(0, new Pair<>(schedule.get(0), List.of()));

        for (int i = 1; i < schedule.size(); i++) {
            List<Integer> endedJobs = new LinkedList<>();
            List<Integer> jobsToStart = new LinkedList<>();
            for (Integer jobIndex : schedule.get(i)) {
                if (!schedule.get(i - 1).contains(jobIndex)) {
                    jobsToStart.add(jobIndex);
                }
            }

            for (Integer jobIndex : schedule.get(i - 1)) {
                if (!schedule.get(i).contains(jobIndex)) {
                    endedJobs.add(jobIndex);
                }
            }
            modes.put(i, new Pair<>(jobsToStart, endedJobs));
        }

        for (int i = 1; i < schedule.size(); i++) {
            for (Integer jobIndex : modes.get(i).getKey()) {
                for (int k = i; k >= 1; k--) {
                    modes.get(k).getValue().forEach(endedJob -> orderMatrix[jobIndex][endedJob] = 1);
                }
            }
        }

        return orderMatrix;
    }

    private static void writeToFile(String fileName, int[][] order,
                                    double estimatedTime) throws IOException {
        try (BufferedWriter bufferedWriter = new BufferedWriter(new FileWriter(fileName.replace(fileName.replaceAll(FILE_NAME_PATTERN, "$1"), OUTPUT_DIR + "/ga_out_")))) {
            bufferedWriter.write(Integer.toString(jobsCount));
            bufferedWriter.newLine();
            for (int i = 0; i < jobIndexToJobIdMap.size(); i++) {
                bufferedWriter.write(jobIndexToJobIdMap.get(i) + " ");
            }
            bufferedWriter.newLine();
            for (int[] ints : order) {
                for (int anInt : ints) {
                    bufferedWriter.write(Integer.toString(anInt));
                    bufferedWriter.write(" ");
                }
                bufferedWriter.newLine();
            }
            bufferedWriter.write(String.format("%.2f", estimatedTime).replaceAll(",", "."));
        }
    }

    private static class GAJob {
        public double busPercent;
        public int jobIndex;

        public GAJob(double busPercent, int jobIndex) {
            this.busPercent = busPercent;
            this.jobIndex = jobIndex;
        }
    }
}
