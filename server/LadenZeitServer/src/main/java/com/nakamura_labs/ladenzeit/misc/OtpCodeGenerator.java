package com.nakamura_labs.ladenzeit.misc;

import java.security.SecureRandom;
import java.util.Random;

public class OtpCodeGenerator {
	private static final String CHARACTERS = "ABCDEFGHKMNPQRSTWXYZ234568";
    private static final Random RANDOM = new SecureRandom();

    public static String generateRawCodeInvalid() {
    	return formatWithHyphens("000000000");
    }
    
    public static String generateRawCode() {
        StringBuilder sb = new StringBuilder(9);
        for (int i = 0; i < 9; i++) {
            sb.append(CHARACTERS.charAt(RANDOM.nextInt(CHARACTERS.length())));
        }
        return sb.toString();
    }

    public static String formatWithHyphens(String rawCode) {
        if (rawCode == null || rawCode.length() != 9) {
            throw new IllegalArgumentException("Raw code must be exactly 9 characters long.");
        }
        return rawCode.substring(0, 3) + "-" + 
               rawCode.substring(3, 6) + "-" + 
               rawCode.substring(6, 9);
    }
}
