package com.nakamura_labs.ladenzeit.entity.common;

import java.time.Instant;

import jakarta.persistence.Column;
import jakarta.persistence.MappedSuperclass;

@MappedSuperclass
public class RowCreatedAt {

	@Column(updatable = false, nullable = false)
	protected Instant createdAt;

	protected RowCreatedAt() {
		this(Instant.now());
	}

	protected RowCreatedAt(Instant now) {
		this.createdAt = now;
	}
}
