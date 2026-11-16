package com.pthanureddy.radio;

import static org.junit.jupiter.api.Assertions.*;
import com.fasterxml.jackson.databind.JsonNode;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;
import java.util.stream.Stream;
import java.util.zip.CRC32;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.Timeout;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.Arguments;
import org.junit.jupiter.params.provider.MethodSource;
import org.junit.jupiter.params.provider.ValueSource;

@Timeout(20)
class RadioLinkAcceptanceTest {
    private static ByteBuffer view(byte[] bytes) { return ByteBuffer.wrap(bytes).order(ByteOrder.BIG_ENDIAN); }

    private static byte[] checksum(byte[] bytes) {
        CRC32 crc = new CRC32();
        crc.update(bytes, 0, 44);
        view(bytes).putInt(44, (int) crc.getValue());
        return bytes;
    }

    private static byte[] sequence(byte[] bytes, int sensor, int sequence) {
        view(bytes).putInt(8, sensor).putInt(12, sequence);
        return checksum(bytes);
    }

    private static void reason(JsonNode result, String expected) {
        assertTrue(result.path("reasons").isArray(), result.toString());
        assertTrue(java.util.stream.StreamSupport.stream(result.path("reasons").spliterator(), false)
                .anyMatch(node -> node.asText().equals(expected)), result.toString());
    }

    @ParameterizedTest
    @ValueSource(strings = {"nominal", "degraded", "critical"})
    void classifiesNativeSenderScenarios(String scenario) throws Exception {
        try (MonitorSession monitor = new MonitorSession(1)) {
            JsonNode result = monitor.exchange(MonitorSession.sourceFrame(scenario));
            assertEquals(scenario, result.path("state").asText());
            assertEquals(1001, result.path("sensor_id").asInt());
            assertEquals(1, result.path("sequence").asInt());
            assertEquals(3_500_000_000L, result.path("center_frequency_hz").asLong());
            assertFalse(result.path("duplicate").asBoolean());
            assertEquals(0, result.path("missing_frames").asInt());
            monitor.awaitSuccess();
        }
    }

    static Stream<Arguments> malformedFrames() {
        return Stream.of(Arguments.of("truncated", "invalid_size"), Arguments.of("oversized", "invalid_size"),
                Arguments.of("magic", "invalid_magic"), Arguments.of("version", "unsupported_version"),
                Arguments.of("crc", "checksum_mismatch"), Arguments.of("bandwidth", "invalid_metric"),
                Arguments.of("evm", "invalid_metric"));
    }

    @ParameterizedTest
    @MethodSource("malformedFrames")
    void rejectsMalformedFrameThenRecovers(String fault, String error) throws Exception {
        try (MonitorSession monitor = new MonitorSession(2)) {
            byte[] invalid = MonitorSession.sourceFrame("nominal");
            switch (fault) {
                case "truncated" -> invalid = Arrays.copyOf(invalid, 47);
                case "oversized" -> invalid = Arrays.copyOf(invalid, 49);
                case "magic" -> { invalid[0] = 0; checksum(invalid); }
                case "version" -> { invalid[4] = 2; checksum(invalid); }
                case "crc" -> invalid[24] ^= 1;
                case "bandwidth" -> { view(invalid).putInt(32, 0); checksum(invalid); }
                case "evm" -> { view(invalid).putShort(40, (short) 1001); checksum(invalid); }
                default -> throw new IllegalArgumentException(fault);
            }
            JsonNode rejected = monitor.exchange(invalid);
            assertEquals("rejected", rejected.path("state").asText());
            assertEquals(error, rejected.path("decode_error").asText());
            JsonNode recovered = monitor.exchange(MonitorSession.sourceFrame("nominal"));
            assertEquals("nominal", recovered.path("state").asText());
            assertFalse(recovered.path("duplicate").asBoolean(), "Decoder rejection must not reach sequence analysis");
            monitor.awaitSuccess();
        }
    }

    @Test
    void tracksDuplicateGapOrderingAndHighWaterMark() throws Exception {
        try (MonitorSession monitor = new MonitorSession(5)) {
            assertEquals("nominal", monitor.exchange(sequence(MonitorSession.sourceFrame("nominal"), 7, 4)).path("state").asText());
            JsonNode duplicate = monitor.exchange(sequence(MonitorSession.sourceFrame("nominal"), 7, 4));
            assertTrue(duplicate.path("duplicate").asBoolean());
            assertEquals("degraded", duplicate.path("state").asText());
            reason(duplicate, "duplicate_sequence");
            JsonNode gap = monitor.exchange(sequence(MonitorSession.sourceFrame("nominal"), 7, 7));
            assertEquals(2, gap.path("missing_frames").asInt());
            reason(gap, "sequence_gap");
            JsonNode reordered = monitor.exchange(sequence(MonitorSession.sourceFrame("nominal"), 7, 6));
            assertTrue(reordered.path("out_of_order").asBoolean());
            reason(reordered, "out_of_order_sequence");
            JsonNode next = monitor.exchange(sequence(MonitorSession.sourceFrame("nominal"), 7, 8));
            assertEquals("nominal", next.path("state").asText());
            assertEquals(0, next.path("missing_frames").asInt(), "Older frame must not move high-water mark backwards");
            monitor.awaitSuccess();
        }
    }

    @Test
    void isolatesSequenceStateBySensor() throws Exception {
        try (MonitorSession monitor = new MonitorSession(3)) {
            monitor.exchange(sequence(MonitorSession.sourceFrame("nominal"), 7, 100));
            JsonNode other = monitor.exchange(sequence(MonitorSession.sourceFrame("nominal"), 8, 1));
            assertEquals("nominal", other.path("state").asText());
            assertFalse(other.path("out_of_order").asBoolean());
            assertEquals("nominal", monitor.exchange(sequence(MonitorSession.sourceFrame("nominal"), 7, 101)).path("state").asText());
            monitor.awaitSuccess();
        }
    }

    @ParameterizedTest
    @ValueSource(strings = {"frequency", "bandwidth", "stale", "future"})
    void rejectsPolicyViolations(String fault) throws Exception {
        try (MonitorSession monitor = new MonitorSession(1)) {
            byte[] frame = MonitorSession.sourceFrame("nominal");
            String expected;
            switch (fault) {
                case "frequency" -> { view(frame).putLong(24, 6_000_000_001L); expected = "center_frequency_out_of_policy"; }
                case "bandwidth" -> { view(frame).putInt(32, 200_000_001); expected = "bandwidth_out_of_policy"; }
                case "stale" -> { view(frame).putLong(16, 0); expected = "stale_or_future_timestamp"; }
                case "future" -> { view(frame).putLong(16, Long.MAX_VALUE); expected = "stale_or_future_timestamp"; }
                default -> throw new IllegalArgumentException(fault);
            }
            JsonNode result = monitor.exchange(checksum(frame));
            assertEquals("rejected", result.path("state").asText());
            reason(result, expected);
            monitor.awaitSuccess();
        }
    }

    static Stream<Arguments> invalidArguments() {
        return Stream.of("radio_link_monitor", "radio_link_sender").flatMap(binary -> Stream.of(
                Arguments.of(binary, "--port", "-1"), Arguments.of(binary, "--port", "65536"),
                Arguments.of(binary, "--port", "7x"), Arguments.of(binary, "--port", "999999999999999999999"),
                Arguments.of(binary, "--port", ""), Arguments.of(binary, "--count", "0"),
                Arguments.of(binary, "--count", "-1"), Arguments.of(binary, "--count", "2x"),
                Arguments.of(binary, "--count", "+2"), Arguments.of(binary, "--count", " 2")));
    }

    @ParameterizedTest
    @MethodSource("invalidArguments")
    void rejectsInvalidNumericArguments(String binary, String option, String value) throws Exception {
        MonitorSession.CommandResult result = MonitorSession.command(binary, option, value);
        assertEquals(2, result.exit());
        assertTrue(result.output().contains(option + " must be an integer"), result.output());
    }

    @Test
    void rejectsInvalidBindAddressBeforeReadiness() throws Exception {
        MonitorSession.CommandResult result = MonitorSession.command("radio_link_monitor", "--bind", "invalid", "--announce-ready");
        assertEquals(2, result.exit());
        assertTrue(result.output().contains("host must be an IPv4 address"));
        assertFalse(result.output().contains("ready port="));
    }
}
