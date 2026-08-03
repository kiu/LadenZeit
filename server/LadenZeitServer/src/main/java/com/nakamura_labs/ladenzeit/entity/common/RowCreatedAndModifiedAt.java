package com.nakamura_labs.ladenzeit.entity.common;

import java.time.Instant;

import jakarta.persistence.Column;
import jakarta.persistence.MappedSuperclass;
import jakarta.persistence.PreUpdate;

@MappedSuperclass
public class RowCreatedAndModifiedAt {

	@Column(updatable = false, nullable = false)
	protected Instant createdAt;

	@Column(nullable = false)
	protected Instant modifiedAt;

	protected RowCreatedAndModifiedAt() {
		this(Instant.now());
	}

	protected RowCreatedAndModifiedAt(Instant now) {
		this.createdAt = now;
		this.modifiedAt = now;
	}

	@PreUpdate
	protected void updateModifiedAt() {
		updateModifiedAt(Instant.now());
	}

	protected void updateModifiedAt(Instant now) {
		this.modifiedAt = now;
	}

}
