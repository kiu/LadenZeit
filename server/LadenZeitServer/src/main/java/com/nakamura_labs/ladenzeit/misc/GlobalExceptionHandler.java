package com.nakamura_labs.ladenzeit.misc;

import java.util.HashMap;
import java.util.Map;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.transaction.TransactionSystemException;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.RestControllerAdvice;
import org.springframework.web.server.ResponseStatusException;
import org.springframework.web.servlet.resource.NoResourceFoundException;

import jakarta.validation.ConstraintViolation;
import jakarta.validation.ConstraintViolationException;

@RestControllerAdvice
public class GlobalExceptionHandler {
	private static final Logger LOG = LoggerFactory.getLogger(GlobalExceptionHandler.class);

	@ExceptionHandler(ResponseStatusException.class)
	public ResponseEntity<Object> handleResponseStatusException(ResponseStatusException ex) {
		if (ex.getStatusCode() == HttpStatus.UNAUTHORIZED) {
			return ResponseEntity.status(HttpStatus.UNAUTHORIZED).build();
		}
		return handleExceptions(ex);
	}

	@ExceptionHandler(NoResourceFoundException.class)
	public ResponseEntity<Void> handleNoResourceFoundExceptio(NoResourceFoundException ex) {
		return ResponseEntity.status(HttpStatus.NOT_FOUND).build();
	}

	@ExceptionHandler(TransactionSystemException.class)
    public ResponseEntity<Map<String, String>> handleJpaValidationFailure(TransactionSystemException ex) {
        Map<String, String> errors = new HashMap<>();
        
        // 1. Dig down to find the root Cause (which is Hibernate's ConstraintViolationException)
        Throwable cause = ex.getRootCause();
        
        if (cause instanceof ConstraintViolationException constraintEx) {
            // 2. Extract each individual failing field and its validation error message
            for (ConstraintViolation<?> violation : constraintEx.getConstraintViolations()) {
                String fieldName = violation.getPropertyPath().toString();
                String errorMessage = violation.getMessage();
                errors.put(fieldName, errorMessage);
            }
            
            // Return a clean 400 Bad Request with the exact validation failures
            return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(errors);
        }

        // Fallback: If it's a transaction failure but NOT caused by validation
        errors.put("error", "Database transaction failed: " + ex.getMessage());
        return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(errors);
    }
	
	@ExceptionHandler(Exception.class)
	public ResponseEntity<Object> handleExceptions(Exception ex) {
		LOG.error("Exception", ex);
		return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).build();
	}
}