package com.pthanureddy.radio;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/** One isolated native monitor, with explicit readiness and bounded cleanup. */
final class MonitorSession implements AutoCloseable {
    private static final ObjectMapper JSON = new ObjectMapper();
    private final Process process;
    private final ExecutorService reader = Executors.newSingleThreadExecutor();
    private final DatagramSocket socket;
    private final BufferedReader output;
    private final int port;

    static Path binary(String name) {
        String suffix = System.getProperty("os.name").startsWith("Windows") ? ".exe" : "";
        Path path = Path.of(System.getProperty("radio.binary.dir"), name + suffix).toAbsolutePath();
        if (!Files.isRegularFile(path)) {
            throw new IllegalStateException("Build native binaries first; not found: " + path);
        }
        return path;
    }

    MonitorSession(int count) throws Exception {
        process = new ProcessBuilder(binary("radio_link_monitor").toString(),
                "--bind", "127.0.0.1", "--port", "0", "--count", Integer.toString(count),
                "--announce-ready").start();
        socket = new DatagramSocket();
        output = new BufferedReader(new InputStreamReader(process.getInputStream(), StandardCharsets.UTF_8));
        try {
            BufferedReader errors = new BufferedReader(new InputStreamReader(process.getErrorStream(), StandardCharsets.UTF_8));
            String ready = reader.submit(errors::readLine).get(5, TimeUnit.SECONDS);
            String prefix = "radio_link_monitor: ready port=";
            if (ready == null || !ready.startsWith(prefix)) {
                throw new IllegalStateException("Monitor failed before readiness: " + ready);
            }
            port = Integer.parseInt(ready.substring(prefix.length()));
            if (port < 1 || port > 65535) throw new IllegalStateException("Invalid ready port: " + port);
        } catch (Exception exception) {
            close();
            throw exception;
        }
    }

    JsonNode exchange(byte[] frame) throws Exception {
        socket.send(new DatagramPacket(frame, frame.length, InetAddress.getByName("127.0.0.1"), port));
        String line = reader.submit(output::readLine).get(5, TimeUnit.SECONDS);
        if (line == null) throw new IllegalStateException("Monitor exited before reporting a datagram");
        JsonNode result = JSON.readTree(line);
        if (!result.isObject() || !result.has("state")) throw new IllegalStateException("Invalid diagnostic: " + line);
        return result;
    }

    void awaitSuccess() throws Exception {
        if (!process.waitFor(5, TimeUnit.SECONDS)) throw new IllegalStateException("Monitor did not exit after count");
        if (process.exitValue() != 0) throw new IllegalStateException("Monitor exit: " + process.exitValue());
        if (output.readLine() != null) throw new IllegalStateException("Unexpected extra diagnostic");
    }

    /** Capture a native timestamp; Java nanoTime has no shared epoch guarantee. */
    static byte[] sourceFrame(String scenario) throws Exception {
        Process sender = null;
        try (DatagramSocket capture = new DatagramSocket(0, InetAddress.getByName("127.0.0.1"))) {
            capture.setSoTimeout(5000);
            sender = new ProcessBuilder(binary("radio_link_sender").toString(), "--port",
                    Integer.toString(capture.getLocalPort()), "--scenario", scenario).redirectErrorStream(true).start();
            DatagramPacket packet = new DatagramPacket(new byte[256], 256);
            capture.receive(packet);
            if (!sender.waitFor(5, TimeUnit.SECONDS) || sender.exitValue() != 0) {
                throw new IllegalStateException("Native sender failed");
            }
            if (packet.getLength() != 48) throw new IllegalStateException("Source frame length is not 48");
            return java.util.Arrays.copyOf(packet.getData(), packet.getLength());
        } finally {
            if (sender != null) stop(sender);
        }
    }

    record CommandResult(int exit, String output) {}

    static CommandResult command(String executable, String... arguments) throws Exception {
        List<String> command = new ArrayList<>();
        command.add(binary(executable).toString());
        command.addAll(List.of(arguments));
        Process child = new ProcessBuilder(command).redirectErrorStream(true).start();
        try {
            if (!child.waitFor(5, TimeUnit.SECONDS)) throw new IllegalStateException("CLI did not terminate: " + command);
            return new CommandResult(child.exitValue(), new String(child.getInputStream().readAllBytes(), StandardCharsets.UTF_8));
        } finally {
            stop(child);
        }
    }

    private static void stop(Process child) throws InterruptedException {
        if (child.isAlive()) {
            child.destroy();
            if (!child.waitFor(1, TimeUnit.SECONDS)) {
                child.destroyForcibly();
                child.waitFor(1, TimeUnit.SECONDS);
            }
        }
    }

    @Override
    public void close() throws Exception {
        socket.close();
        try {
            stop(process);
        } finally {
            reader.shutdownNow();
            output.close();
        }
    }
}
