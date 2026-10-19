public final class JniExceptionCheck {
    private static native void invoke(int mode);
    public static void main(String[] args) {
        System.load(args[0]);
        invoke(0);
        for (int mode = 1; mode <= 3; mode++) {
            try {
                invoke(mode);
                throw new AssertionError("Expected Java exception for mode " + mode);
            } catch (RuntimeException error) {
                String expected = switch (mode) {
                    case 1 -> "VK_ERROR_DEVICE_LOST";
                    case 2 -> "Unknown native renderer exception";
                    default -> "existing Java exception";
                };
                if (!error.getMessage().contains(expected)) throw new AssertionError(error);
                if (mode == 3 && !(error instanceof IllegalStateException)) throw new AssertionError(error);
            }
            invoke(0);
        }
        System.out.println("PASS: native device-lost, unknown exception and existing Java exception handled; JVM remains alive");
    }
}
