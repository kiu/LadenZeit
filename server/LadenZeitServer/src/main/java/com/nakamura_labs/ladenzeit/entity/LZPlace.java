package com.nakamura_labs.ladenzeit.entity;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

import com.nakamura_labs.ladenzeit.entity.common.RowCreatedAt;

import jakarta.persistence.CascadeType;
import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.Id;
import jakarta.persistence.Index;
import jakarta.persistence.JoinColumn;
import jakarta.persistence.OneToMany;
import jakarta.persistence.Table;

@Entity
@Table(indexes = { @Index(name = "idx_created_at", columnList = "created_at") })
public class LZPlace extends RowCreatedAt {

	@Id
	@Column(updatable = false, nullable = false)
	private String id;

	@Column(updatable = false, nullable = false)
	private String name;

	@OneToMany(cascade = CascadeType.ALL, orphanRemoval = true)
	@JoinColumn(name = "place_id")
	private List<LZPlaceHour> lzPlaceHours = new ArrayList<>();

	public void setId(String id) {
		this.id = id;
	}

	public String getName() {
		return name;
	}

	public void setName(String name) {
		this.name = name;
	}

	public void addPlaceHour(LZPlaceHour ph) {
		this.lzPlaceHours.add(ph);
	}

	public List<LZPlaceHour> getPlaceHours() {
		return this.lzPlaceHours;
	}

	public void sortPlaceHours() {
		if (this.lzPlaceHours != null) {
			this.lzPlaceHours.sort(
					Comparator.comparing(LZPlaceHour::getFromInteger, Comparator.nullsLast(Comparator.naturalOrder())));
		}
	}

	public LZPlace() {
		super();
	}

}